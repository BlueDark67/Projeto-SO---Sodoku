/*
 * servidor/src/util-stream-server.c
 * 
 * Lógica de processamento do servidor - Handler de clientes
 * 
 * Este ficheiro contém a função str_echo() que é executada por cada
 * processo-filho criado pelo servidor para atender um cliente.
 * 
 * Responsabilidades:
 * - Sincronizar clientes usando semáforos (barreira)
 * - Receber pedidos de jogos dos clientes
 * - Enviar jogos do banco de dados
 * - Receber soluções dos clientes
 * - Verificar correção das soluções
 * - Registar todos os eventos em log
 * - Gerir erros e desconexões
 */

#include "util.h"
#include <string.h>
#include <errno.h>           // Para EAGAIN, EWOULDBLOCK
#include <sys/socket.h>      // Para setsockopt, SOL_SOCKET
#include <sys/time.h>        // Para struct timeval

// Headers necessários para o protocolo e gestão de jogos
#include "protocolo.h"        // Definição de mensagens e tipos
#include "config_servidor.h"  // Configurações
#include "jogos.h"            // Verificação de soluções
#include "logs.h"             // Sistema de logging
#include "servidor.h"         // Estruturas compartilhadas

/**
 * @brief Função principal executada por cada processo-filho
 * 
 * Esta função implementa o protocolo de comunicação com o cliente:
 * 1. Sincroniza com outros clientes (barreira)
 * 2. Aguarda pedido de jogo
 * 3. Envia jogo aleatório do banco de dados
 * 4. Recebe solução do cliente
 * 5. Verifica e envia resultado
 * 6. Repete até o cliente desconectar
 * 
 * @param sockfd Socket de comunicação com o cliente
 * @param jogos Array de jogos disponíveis
 * @param numJogos Número total de jogos carregados
 * @param dados Ponteiro para memória partilhada (sincronização)
 * @param maxLinha Tamanho máximo do buffer (configurável)
 * @param timeoutCliente Timeout em segundos para operações de socket
 */
void str_echo(int sockfd, Jogo jogos[], int numJogos, DadosPartilhados *dados, int maxLinha, int timeoutCliente)
{
    int n;
    MensagemSudoku msg_recebida;
    MensagemSudoku msg_resposta;

    // Validar tamanho de buffer configurado
    if (maxLinha < 256) {
        printf("[AVISO] maxLinha configurado (%d) é muito pequeno, usando 512\n", maxLinha);
        maxLinha = 512;
    }

    /* NOTA: O timeout será aplicado APÓS a sincronização
     * Durante a barreira, o cliente pode estar à espera indefinidamente
     * até que outro cliente se conecte. Não faz sentido ter timeout aqui.
     */

    /* ========================================
     * INCREMENTAR CONTADOR DE CLIENTES
     * ========================================
     * Nota: A sincronização acontece no PEDIR_JOGO,
     * não na conexão inicial. Isto permite que clientes
     * que querem jogar novamente se sincronizem com
     * novos clientes que conectam.
     */
    
    sem_wait(&dados->mutex);
    dados->numClientes++;
    int total_conectados = dados->numClientes;
    sem_post(&dados->mutex);
    
    printf("Servidor: Cliente conectado. Total: %d conectados\n", total_conectados);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), 
             "Cliente conectado - Total: %d conectados", 
             total_conectados);
    registarEvento(0, EVT_SERVIDOR_INICIADO, log_msg);

    /* Aplicar timeout de socket */
    struct timeval timeout;
    timeout.tv_sec = timeoutCliente;
    timeout.tv_usec = 0;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        registarEvento(0, EVT_ERRO_GERAL, "Falha ao configurar SO_RCVTIMEO no socket do cliente");
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        registarEvento(0, EVT_ERRO_GERAL, "Falha ao configurar SO_SNDTIMEO no socket do cliente");
    }
    
    snprintf(log_msg, sizeof(log_msg), 
             "Timeout de socket ativado: %d segundos (%d minutos)", 
             timeoutCliente, timeoutCliente / 60);
    registarEvento(0, EVT_SERVIDOR_INICIADO, log_msg);
    printf("[DEBUG] Timeout ativado: %d segundos\n", timeoutCliente);

    // Lógica principal do servidor
    for (;;)
    {
        // 1. LER A MENSAGEM DO CLIENTE
        // Usamos readn para garantir que lemos a estrutura inteira
        n = readn(sockfd, (char *)&msg_recebida, sizeof(MensagemSudoku));

        if (n == 0)
        {
            // Cliente desligou-se (EOF)
            goto cleanup_e_sair;
        }
        else if (n < 0)
        {
            // Verificar se foi timeout
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                char log_timeout[256];
                snprintf(log_timeout, sizeof(log_timeout), 
                         "Cliente #%d não respondeu em %d segundos (timeout)", 
                         msg_recebida.idCliente, timeoutCliente);
                registarEvento(msg_recebida.idCliente, EVT_ERRO_GERAL, log_timeout);
                printf("[TIMEOUT] Cliente não respondeu, a fechar ligação.\n");
                goto cleanup_e_sair;
            }
            err_dump("str_echo: erro a ler a mensagem");
            goto cleanup_e_sair;
        }
        else if (n != sizeof(MensagemSudoku))
        {
            err_dump("str_echo: erro a ler a mensagem (bytes inesperados)");
            goto cleanup_e_sair;
        }

        // Limpa a estrutura de resposta
        bzero(&msg_resposta, sizeof(MensagemSudoku));
        msg_resposta.idCliente = msg_recebida.idCliente; // Responde ao mesmo cliente

        // 2. PROCESSAR A MENSAGEM
        switch (msg_recebida.tipo)
        {
        case PEDIR_JOGO:
            // ========================================
            // RE-SINCRONIZAÇÃO: Garantir que há 2 jogadores
            // ========================================
            printf("Servidor: Cliente %d pediu um jogo.\n", msg_recebida.idCliente);
            
            char log_msg[256];
            
            // Verificar se há alguém já à espera de novo jogo
            sem_wait(&dados->mutex);
            
            if (dados->clientesEmEspera > 0) {
                // CASO 1: Há alguém à espera - formar par
                dados->clientesEmEspera--;
                sem_post(&dados->mutex);
                
                printf("Servidor: Par formado para novo jogo! Ambos avançam.\n");
                snprintf(log_msg, sizeof(log_msg), 
                         "Par formado para novo jogo - Cliente #%d pode avançar", 
                         msg_recebida.idCliente);
                registarEvento(msg_recebida.idCliente, EVT_SERVIDOR_INICIADO, log_msg);
                
                // Desbloquear o outro cliente
                sem_post(&dados->barreira);
            } else {
                // CASO 2: Ninguém à espera - esperar por outro jogador
                dados->clientesEmEspera++;
                int espera = dados->clientesEmEspera;
                sem_post(&dados->mutex);
                
                printf("Servidor: Cliente %d aguardando outro jogador para novo jogo...\n", 
                       msg_recebida.idCliente);
                snprintf(log_msg, sizeof(log_msg), 
                         "Cliente #%d aguardando par para novo jogo (%d em espera)", 
                         msg_recebida.idCliente, espera);
                registarEvento(msg_recebida.idCliente, EVT_SERVIDOR_INICIADO, log_msg);
                
                // Bloquear até outro cliente pedir jogo
                sem_wait(&dados->barreira);
                
                printf("Servidor: Cliente %d desbloqueado! Novo jogo iniciado.\n", 
                       msg_recebida.idCliente);
                snprintf(log_msg, sizeof(log_msg), 
                         "Cliente #%d desbloqueado - novo jogo iniciado", 
                         msg_recebida.idCliente);
                registarEvento(msg_recebida.idCliente, EVT_SERVIDOR_INICIADO, log_msg);
            }
            
            // Agora sim, enviar o jogo (ambos sincronizados)
            if (numJogos > 0)
            {
                // Contar células preenchidas no jogo
                int celulas_preenchidas = 0;
                for (int i = 0; i < 81; i++) {
                    if (jogos[0].tabuleiro[i] != '0') celulas_preenchidas++;
                }
                
                snprintf(log_msg, sizeof(log_msg), 
                         "Cliente #%d pediu jogo - Enviado Jogo #%d (%d células preenchidas)", 
                         msg_recebida.idCliente, jogos[0].idjogo, celulas_preenchidas);
                registarEvento(msg_recebida.idCliente, EVT_JOGO_PEDIDO, log_msg);
                
                msg_resposta.tipo = ENVIAR_JOGO;
                msg_resposta.idJogo = jogos[0].idjogo;
                strncpy(msg_resposta.tabuleiro, jogos[0].tabuleiro, sizeof(msg_resposta.tabuleiro) - 1);
                msg_resposta.tabuleiro[sizeof(msg_resposta.tabuleiro) - 1] = '\0';
                
                snprintf(log_msg, sizeof(log_msg), 
                         "Jogo #%d enviado para Cliente #%d", 
                         jogos[0].idjogo, msg_recebida.idCliente);
                registarEvento(msg_recebida.idCliente, EVT_JOGO_ENVIADO, log_msg);
            }
            else
            {
                // Tratar erro (não há jogos)
                snprintf(log_msg, sizeof(log_msg), 
                         "ERRO: Cliente #%d pediu jogo mas não há jogos disponíveis", 
                         msg_recebida.idCliente);
                registarEvento(msg_recebida.idCliente, EVT_ERRO_GERAL, log_msg);
            }
            break;

        case ENVIAR_SOLUCAO:
            // Lógica de Verificação de Solução
            printf("Servidor: Cliente %d enviou solução para jogo %d.\n",
                   msg_recebida.idCliente, msg_recebida.idJogo);
            
            // VALIDAÇÃO: Verificar tamanho do tabuleiro
            size_t tam_tabuleiro = strnlen(msg_recebida.tabuleiro, sizeof(msg_recebida.tabuleiro));
            if (tam_tabuleiro != 81) {
                snprintf(log_msg, sizeof(log_msg), 
                         "ERRO: Cliente #%d enviou tabuleiro com tamanho inválido (%zu chars, esperado 81)",
                         msg_recebida.idCliente, tam_tabuleiro);
                registarEvento(msg_recebida.idCliente, EVT_ERRO_GERAL, log_msg);
                
                msg_resposta.tipo = RESPOSTA_SOLUCAO;
                msg_resposta.idJogo = msg_recebida.idJogo;
                strncpy(msg_resposta.resposta, "Erro: Tabuleiro inválido", sizeof(msg_resposta.resposta) - 1);
                msg_resposta.resposta[sizeof(msg_resposta.resposta) - 1] = '\0';
                break;
            }
            
            // VALIDAÇÃO: Verificar caracteres válidos (apenas 0-9)
            int caractere_invalido = 0;
            for (int i = 0; i < 81; i++) {
                if (msg_recebida.tabuleiro[i] < '0' || msg_recebida.tabuleiro[i] > '9') {
                    caractere_invalido = 1;
                    break;
                }
            }
            
            if (caractere_invalido) {
                snprintf(log_msg, sizeof(log_msg), 
                         "ERRO: Cliente #%d enviou tabuleiro com caracteres inválidos",
                         msg_recebida.idCliente);
                registarEvento(msg_recebida.idCliente, EVT_ERRO_GERAL, log_msg);
                
                msg_resposta.tipo = RESPOSTA_SOLUCAO;
                msg_resposta.idJogo = msg_recebida.idJogo;
                strncpy(msg_resposta.resposta, "Erro: Caracteres inválidos", sizeof(msg_resposta.resposta) - 1);
                msg_resposta.resposta[sizeof(msg_resposta.resposta) - 1] = '\0';
                break;
            }
            
            // Contar células preenchidas na solução
            int celulas_solucao = 0;
            for (int i = 0; i < 81; i++) {
                if (msg_recebida.tabuleiro[i] != '0') celulas_solucao++;
            }
            
            snprintf(log_msg, sizeof(log_msg), 
                     "Cliente #%d enviou solução para Jogo #%d (%d células preenchidas)", 
                     msg_recebida.idCliente, msg_recebida.idJogo, celulas_solucao);
            registarEvento(msg_recebida.idCliente, EVT_SOLUCAO_RECEBIDA, log_msg);

            // Encontrar o jogo correspondente
            Jogo *jogo_atual = NULL;
            for (int i = 0; i < numJogos; i++)
            {
                if (jogos[i].idjogo == msg_recebida.idJogo)
                {
                    jogo_atual = &jogos[i];
                    break;
                }
            }

            msg_resposta.tipo = RESPOSTA_SOLUCAO;
            msg_resposta.idJogo = msg_recebida.idJogo;

            if (jogo_atual != NULL)
            {
                // Usar a tua função da Fase 1
                ResultadoVerificacao res = verificarSolucao(msg_recebida.tabuleiro, jogo_atual->solucao);

                if (res.correto)
                {
                    strcpy(msg_resposta.resposta, "Certo");
                    snprintf(log_msg, sizeof(log_msg), 
                             "✓ SOLUÇÃO CORRETA - Cliente #%d resolveu Jogo #%d (81 células corretas)", 
                             msg_recebida.idCliente, msg_recebida.idJogo);
                    registarEvento(msg_recebida.idCliente, EVT_SOLUCAO_CORRETA, log_msg);
                }
                else
                {
                    strcpy(msg_resposta.resposta, "Errado");
                    snprintf(log_msg, sizeof(log_msg), 
                             "✗ SOLUÇÃO INCORRETA - Cliente #%d, Jogo #%d: %d erros, %d acertos de 81 células", 
                             msg_recebida.idCliente, msg_recebida.idJogo, 
                             res.numerosErrados, res.numerosCertos);
                    registarEvento(msg_recebida.idCliente, EVT_SOLUCAO_ERRADA, log_msg);
                }
            }
            else
            {
                strcpy(msg_resposta.resposta, "Erro: Jogo Nao Encontrado");
                snprintf(log_msg, sizeof(log_msg), 
                         "ERRO: Cliente #%d tentou validar Jogo #%d inexistente", 
                         msg_recebida.idCliente, msg_recebida.idJogo);
                registarEvento(msg_recebida.idCliente, EVT_ERRO_GERAL, log_msg);
            }
            break;

        default:
            printf("Servidor: Recebeu tipo de mensagem desconhecido (%d)\n", msg_recebida.tipo);
            // Não envia resposta se não entender o pedido
            continue;
        }

        // 3. ENVIAR A RESPOSTA AO CLIENTE
        // Usamos writen para garantir que enviamos a estrutura inteira
        if (writen(sockfd, (char *)&msg_resposta, sizeof(MensagemSudoku)) != sizeof(MensagemSudoku))
        {
            err_dump("str_echo: erro a escrever resposta");
        }
    }
    
cleanup_e_sair:
    /* ========================================
     * CLEANUP: DECREMENTAR CONTADOR DE CLIENTES
     * ========================================
     * Quando o cliente desconecta (fim do loop ou erro),
     * devemos decrementar o contador de clientes conectados.
     * 
     * IMPORTANTE: Não decrementamos clientesEmEspera porque:
     * - Se o cliente estava a jogar (não em espera), clientesEmEspera
     *   já não o contava
     * - Se o cliente estava em espera e desconecta (timeout/erro),
     *   ele SAI do sem_wait() com erro e não chega aqui
     * 
     * Esta lógica garante que:
     * 1. Estatísticas de "clientes ligados" são corretas
     * 2. clientesEmEspera reflete sempre a realidade
     * 3. Não há sinais acumulados na barreira
     */
    
    // Zona crítica: decrementar contador de clientes conectados
    sem_wait(&dados->mutex);
    dados->numClientes--;
    int clientes_restantes = dados->numClientes;
    int em_espera = dados->clientesEmEspera;
    sem_post(&dados->mutex);
    
    printf("[DEBUG] Cliente desconectado.\n");
    printf("        Clientes conectados: %d | Em espera: %d\n", 
           clientes_restantes, em_espera);
    
    char log_desconexao[256];
    snprintf(log_desconexao, sizeof(log_desconexao), 
             "Cliente desconectado - Conectados: %d, Em espera: %d", 
             clientes_restantes, em_espera);
    registarEvento(0, EVT_CLIENTE_DESCONECTADO, log_desconexao);
}
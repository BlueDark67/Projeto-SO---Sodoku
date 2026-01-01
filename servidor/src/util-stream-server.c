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
     * SINCRONIZAÇÃO ENTRE CLIENTES
     * ======================================== */
    
    // Zona crítica: incrementar contador de clientes
    sem_wait(&dados->mutex);  // Entrar na secção crítica
    dados->numClientes++;
    int meus_clientes = dados->numClientes;
    sem_post(&dados->mutex);  // Sair da secção crítica

    printf("[DEBUG] Cliente conectado. Total: %d\n", meus_clientes);

    // Barreira de sincronização: aguardar mínimo de 2 clientes
    if (meus_clientes < 2)
    {
        printf("Servidor: Cliente 1 à espera do Cliente 2 (sem timeout)...\n");
        // Primeiro cliente: bloqueia na barreira
        // Fica em espera até outro cliente fazer sem_post na barreira
        sem_wait(&dados->barreira);
        printf("Servidor: Cliente 1 desbloqueado! O jogo vai começar.\n");
    }
    else
    {
        printf("Servidor: Cliente 2 chegou! A desbloquear Cliente 1.\n");
        // Cliente 2 ou posterior: liberta o primeiro cliente
        sem_post(&dados->barreira);
    }

    /* Aplicar timeout de socket APÓS sincronização */
    struct timeval timeout;
    timeout.tv_sec = timeoutCliente;
    timeout.tv_usec = 0;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        registarEvento(0, EVT_ERRO_GERAL, "Falha ao configurar SO_RCVTIMEO no socket do cliente");
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        registarEvento(0, EVT_ERRO_GERAL, "Falha ao configurar SO_SNDTIMEO no socket do cliente");
    }
    
    char log_timeout[256];
    snprintf(log_timeout, sizeof(log_timeout), "Timeout de socket ativado: %d segundos (%d minutos)", timeoutCliente, timeoutCliente / 60);
    registarEvento(0, EVT_SERVIDOR_INICIADO, log_timeout);
    printf("[DEBUG] Timeout ativado: %d segundos\n", timeoutCliente);

    // Lógica principal do servidor
    for (;;)
    {
        // 1. LER A MENSAGEM DO CLIENTE
        // Usamos readn para garantir que lemos a estrutura inteira
        n = readn(sockfd, (char *)&msg_recebida, sizeof(MensagemSudoku));

        if (n == 0)
        {
            return; // Cliente desligou-se (EOF)
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
                return;
            }
            err_dump("str_echo: erro a ler a mensagem");
            return; // Erro
        }
        else if (n != sizeof(MensagemSudoku))
        {
            err_dump("str_echo: erro a ler a mensagem (bytes inesperados)");
            return; // Erro
        }

        // Limpa a estrutura de resposta
        bzero(&msg_resposta, sizeof(MensagemSudoku));
        msg_resposta.idCliente = msg_recebida.idCliente; // Responde ao mesmo cliente

        // 2. PROCESSAR A MENSAGEM
        switch (msg_recebida.tipo)
        {
        case PEDIR_JOGO:
            // Lógica de Pedido de Jogo
            printf("Servidor: Cliente %d pediu um jogo.\n", msg_recebida.idCliente);
            
            char log_msg[256];
            // LÓGICA SIMPLES: Envia sempre o primeiro jogo (jogo[0])
            // (Podes tornar isto mais complexo depois, ex: aleatório)
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
}
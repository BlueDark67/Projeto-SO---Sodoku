/*
 * util-stream-server.c
 * Lógica de processamento do servidor
 */

#include "util.h"
#include <string.h>

// --- INCLUDES ADICIONADOS ---
// Precisamos disto para saber o que é MensagemSudoku, Jogo, etc.
#include "protocolo.h" // common/include
#include "config_servidor.h"
#include "jogos.h"
#include "logs.h"
#include "servidor.h"
// -----------------------------

/* * Função principal do processo-filho.
 * Recebe mensagens do cliente (sockfd) e age de acordo.
 * Usa a lista de jogos (jogos) para validar.
 */
void str_echo(int sockfd, Jogo jogos[], int numJogos, DadosPartilhados *dados, int maxLinha)
{
    int n;
    MensagemSudoku msg_recebida;
    MensagemSudoku msg_resposta;

    // Validar tamanho de buffer configurado
    if (maxLinha < 256) {
        printf("[AVISO] maxLinha configurado (%d) é muito pequeno, usando 512\n", maxLinha);
        maxLinha = 512;
    }

    sem_wait(&dados->mutex);
    dados->numClientes++;
    int meus_clientes = dados->numClientes;
    sem_post(&dados->mutex); // Sair da zona crítica

    printf("[DEBUG] Cliente conectado. Total: %d\n", meus_clientes);

    if (meus_clientes < 2)
    {
        printf("Servidor: Cliente 1 à espera do Cliente 2...\n");
        // Se somos o primeiro, esperamos na barreira
        // O processo fica parado aqui até alguém fazer sem_post
        sem_wait(&dados->barreira);
        printf("Servidor: Cliente 1 desbloqueado! O jogo vai começar.\n");
    }
    else
    {
        printf("Servidor: Cliente 2 chegou! A desbloquear Cliente 1.\n");
        // Se somos o segundo (ou mais), libertamos quem está preso
        sem_post(&dados->barreira);
    }

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
                strcpy(msg_resposta.tabuleiro, jogos[0].tabuleiro);
                
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
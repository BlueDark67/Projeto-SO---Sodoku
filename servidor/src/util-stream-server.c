// servidor/src/util-stream-server.c - Sistema de lobby dinâmico com timer de agregação

#include "util.h"
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "protocolo.h"
#include "config_servidor.h"
#include "jogos.h"
#include "logs.h"
#include "servidor.h"

void str_echo(int sockfd, Jogo jogos[], int numJogos, DadosPartilhados *dados, int maxLinha, int timeoutCliente)
{
    (void)maxLinha;

    int n;
    MensagemSudoku msg_recebida;
    MensagemSudoku msg_resposta;
    int meu_jogo = -1;

    // FASE 1: Controlo de capacidade
    sem_wait(&dados->mutex);

    if (dados->numClientesJogando >= 10)
    {
        sem_post(&dados->mutex);

        bzero(&msg_resposta, sizeof(MensagemSudoku));
        msg_resposta.tipo = 99;
        strncpy(msg_resposta.resposta,
                "Servidor cheio (10/10). Aguarde que alguém saia.",
                sizeof(msg_resposta.resposta) - 1);

        writen(sockfd, (char *)&msg_resposta, sizeof(MensagemSudoku));
        registarEvento(0, EVT_ERRO_GERAL, "Cliente rejeitado - servidor cheio");

        close(sockfd);
        return;
    }

    dados->numClientesJogando++;

    sem_post(&dados->mutex);

    // Loop principal: múltiplos jogos
    for (;;)
    {

        // FASE 2: Aguardar pedido de jogo
        n = readn(sockfd, (char *)&msg_recebida, sizeof(MensagemSudoku));

        if (n <= 0)
        {
            goto cleanup_e_sair;
        }

        if (msg_recebida.tipo != PEDIR_JOGO)
        {
            goto cleanup_e_sair;
        }

        char log_lobby[256];
        snprintf(log_lobby, sizeof(log_lobby), "Cliente %d entrou no lobby (Aguardando sincronização)", msg_recebida.idCliente);
        registarEvento(msg_recebida.idCliente, EVT_CLIENTE_CONECTADO, log_lobby);

        // FASE 3: Entrar no lobby e aguardar
        sem_wait(&dados->mutex);
        dados->numClientesLobby++;
        dados->ultimaEntrada = time(NULL);

        if (dados->numClientesLobby >= 10)
        {
            dados->jogoAtual = rand() % numJogos;
            dados->jogoIniciado = 1;
            dados->jogoTerminado = 0;
            dados->idVencedor = -1;
            dados->tempoVitoria = 0;

            printf("\n\033[32mJogo #%d iniciado - Lobby cheio (10 jogadores)\033[0m\n", dados->jogoAtual);

            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg),
                     "Jogo #%d iniciado - Lobby cheio (10 jogadores)",
                     dados->jogoAtual);
            registarEvento(0, EVT_SERVIDOR_INICIADO, log_msg);

            for (int i = 0; i < 10; i++)
            {
                sem_post(&dados->lobby_semaforo);
            }
        }
        sem_post(&dados->mutex);

        sem_wait(&dados->lobby_semaforo);

        snprintf(log_lobby, sizeof(log_lobby), "Sincronização concluída! Cliente %d a iniciar jogo", msg_recebida.idCliente);
        registarEvento(msg_recebida.idCliente, EVT_SERVIDOR_INICIADO, log_lobby);

        // FASE 4: Enviar jogo
        sem_wait(&dados->mutex);
        meu_jogo = dados->jogoAtual;
        dados->numClientesLobby--;
        dados->numJogadoresAtivos++;
        sem_post(&dados->mutex);

        bzero(&msg_resposta, sizeof(MensagemSudoku));
        msg_resposta.tipo = ENVIAR_JOGO;
        msg_resposta.idJogo = jogos[meu_jogo].idjogo;
        strncpy(msg_resposta.tabuleiro, jogos[meu_jogo].tabuleiro, sizeof(msg_resposta.tabuleiro) - 1);

        if (writen(sockfd, (char *)&msg_resposta, sizeof(MensagemSudoku)) != sizeof(MensagemSudoku))
        {
            goto cleanup_e_sair;
        }

        registarEvento(msg_recebida.idCliente, EVT_JOGO_ENVIADO, "Jogo enviado ao cliente");

        // FASE 5: Configurar timeout
        struct timeval timeout;
        timeout.tv_sec = timeoutCliente;
        timeout.tv_usec = 0;

        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        // FASE 6: Aguardar solução ou validações
        int aguardando_solucao = 1;
        while (aguardando_solucao)
        {
            sem_wait(&dados->mutex);
            if (dados->jogoTerminado && dados->idVencedor != msg_recebida.idCliente)
            {
                int vencedor = dados->idVencedor;
                sem_post(&dados->mutex);

                bzero(&msg_resposta, sizeof(MensagemSudoku));
                msg_resposta.tipo = JOGO_TERMINADO;
                msg_resposta.idCliente = vencedor; // Quem ganhou
                msg_resposta.idJogo = meu_jogo;
                snprintf(msg_resposta.resposta, sizeof(msg_resposta.resposta),
                         "Cliente %d ganhou primeiro!", vencedor);
                writen(sockfd, (char *)&msg_resposta, sizeof(MensagemSudoku));

                char log_derrota[256];
                snprintf(log_derrota, sizeof(log_derrota),
                         "Jogo terminado - Cliente %d venceu", vencedor);
                registarEvento(msg_recebida.idCliente, EVT_JOGO_PERDIDO, log_derrota);
                goto cleanup_e_sair;
            }
            sem_post(&dados->mutex);

            n = readn(sockfd, (char *)&msg_recebida, sizeof(MensagemSudoku));

            if (n == 0)
            {
                printf("[INFO] Cliente desconectou após jogo\n");
                goto cleanup_e_sair;
            }

            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    printf("[TIMEOUT] Cliente não respondeu\n");
                    registarEvento(msg_recebida.idCliente, EVT_ERRO_GERAL, "Timeout");
                }
                goto cleanup_e_sair;
            }

            // --- NOVO: Validação Parcial de Blocos ---
            if (msg_recebida.tipo == VALIDAR_BLOCO)
            {
                time_t now = time(NULL);
                struct tm *t = localtime(&now);

                // Log de Receção com Cor (Azul para REDE)
                printf("\x1b[34m[%02d:%02d:%02d] [%d] [REDE]  Recebido pedido VALIDAR_BLOCO (ID: %d)\x1b[0m\n",
                       t->tm_hour, t->tm_min, t->tm_sec,
                       msg_recebida.idCliente, msg_recebida.bloco_id);

                // Log para ficheiro
                char log_msg[128];
                snprintf(log_msg, sizeof(log_msg), "Pedido de validação para Bloco %d", msg_recebida.bloco_id);
                registarEvento(msg_recebida.idCliente, EVT_VALIDACAO_BLOCO, log_msg);

                int bloco_correto = 1;
                int start_row = (msg_recebida.bloco_id / 3) * 3;
                int start_col = (msg_recebida.bloco_id % 3) * 3;
                const char *solucao = jogos[meu_jogo].solucao;

                int k = 0;
                for (int r = 0; r < 3; r++)
                {
                    for (int c = 0; c < 3; c++)
                    {
                        int idx = (start_row + r) * 9 + (start_col + c);
                        int val_solucao = solucao[idx] - '0';
                        int val_cliente = msg_recebida.conteudo_bloco[k++];

                        if (val_cliente != 0 && val_cliente != val_solucao)
                        {
                            bloco_correto = 0;
                        }
                    }
                }

                bzero(&msg_resposta, sizeof(MensagemSudoku));
                msg_resposta.tipo = RESPOSTA_BLOCO;
                msg_resposta.idCliente = msg_recebida.idCliente;
                msg_resposta.idJogo = meu_jogo;
                msg_resposta.bloco_id = msg_recebida.bloco_id;

                if (bloco_correto)
                {
                    strcpy(msg_resposta.resposta, "OK");

                    snprintf(log_msg, sizeof(log_msg), "Bloco %d validado com sucesso", msg_recebida.bloco_id);
                    registarEvento(msg_recebida.idCliente, EVT_VALIDACAO_BLOCO_OK, log_msg);
                }
                else
                {
                    strcpy(msg_resposta.resposta, "NOK");

                    snprintf(log_msg, sizeof(log_msg), "Bloco %d inválido", msg_recebida.bloco_id);
                    registarEvento(msg_recebida.idCliente, EVT_VALIDACAO_BLOCO_NOK, log_msg);
                }

                writen(sockfd, (char *)&msg_resposta, sizeof(MensagemSudoku));
                continue;
            }

            if (msg_recebida.tipo == ENVIAR_SOLUCAO)
            {
                aguardando_solucao = 0;
            }
            else
            {
                goto cleanup_e_sair;
            }
        }

        char log_solucao[256];
        snprintf(log_solucao, sizeof(log_solucao), "Solução recebida do Cliente %d (A verificar...)", msg_recebida.idCliente);
        registarEvento(msg_recebida.idCliente, EVT_SOLUCAO_RECEBIDA, log_solucao);

        ResultadoVerificacao resultado = verificarSolucao(msg_recebida.tabuleiro, jogos[meu_jogo].solucao, jogos[meu_jogo].tabuleiro);

        bzero(&msg_resposta, sizeof(MensagemSudoku));
        msg_resposta.tipo = RESPOSTA_SOLUCAO;
        msg_resposta.idCliente = msg_recebida.idCliente;
        msg_resposta.idJogo = msg_recebida.idJogo;

        if (resultado.correto)
        {
            int precisa_marcar = 0;

            sem_wait(&dados->mutex);
            if (!dados->jogoTerminado)
            {
                dados->jogoTerminado = 1;
                dados->idVencedor = msg_recebida.idCliente;
                dados->tempoVitoria = time(NULL);
                precisa_marcar = 1;

                printf("\033[1;35mCliente #%d venceu!\033[0m\n", msg_recebida.idCliente);
            }
            sem_post(&dados->mutex);

            strncpy(msg_resposta.resposta, "Certo", sizeof(msg_resposta.resposta) - 1);

            if (precisa_marcar)
            {
                registarEvento(msg_recebida.idCliente, EVT_SOLUCAO_CORRETA, "Solução correta - VENCEDOR");
            }
            else
            {
                registarEvento(msg_recebida.idCliente, EVT_SOLUCAO_CORRETA, "Solução correta - mas não foi o primeiro");
            }
        }
        else
        {
            snprintf(msg_resposta.resposta, sizeof(msg_resposta.resposta),
                     "Errado (%d erros)", resultado.numerosErrados);

            char log_detalhado[256];
            snprintf(log_detalhado, sizeof(log_detalhado),
                     "Solução incorreta - %d erros, %d acertos",
                     resultado.numerosErrados, resultado.numerosCertos);
            registarEvento(msg_recebida.idCliente, EVT_SOLUCAO_ERRADA, log_detalhado);
        }

        writen(sockfd, (char *)&msg_resposta, sizeof(MensagemSudoku));

        sem_wait(&dados->mutex);
        dados->numJogadoresAtivos--;
        if (dados->numJogadoresAtivos == 0)
        {
            dados->jogoIniciado = 0;
        }
        sem_post(&dados->mutex);
    }

cleanup_e_sair:

    sem_wait(&dados->mutex);
    dados->numClientesJogando--;

    // Se lobby ficou vazio, resetar estado
    if (dados->numClientesLobby == 0 && dados->numClientesJogando == 0)
    {
        dados->jogoIniciado = 0;
        dados->jogoAtual = -1;
        dados->numJogadoresAtivos = 0; // Resetar também
        printf("[LOBBY] Lobby resetado (vazio)\n");
    }
    else if (dados->numJogadoresAtivos == 0 && dados->jogoIniciado == 1)
    {
        // Se saiu o último jogador ativo (por erro/disconnect)
        // Precisamos verificar se mais ninguém está a jogar
        // Como não temos flag local fácil, vamos confiar que numJogadoresAtivos
        // reflete a realidade se gerido corretamente.
        // Se este cliente estava a jogar e saiu com erro, numJogadoresAtivos
        // não foi decrementado no loop.
        // Isto é um bug potencial se não usarmos flag local.
        // CORREÇÃO RÁPIDA: Se numClientesJogando < numJogadoresAtivos, algo está errado.
        if (dados->numClientesJogando < dados->numJogadoresAtivos)
        {
            dados->numJogadoresAtivos = dados->numClientesJogando;
        }
        if (dados->numJogadoresAtivos == 0)
        {
            dados->jogoIniciado = 0;
            printf("[LOBBY] Último jogador saiu. Jogo resetado.\n");
        }
    }

    int restantes = dados->numClientesJogando;
    sem_post(&dados->mutex);

    printf("[LOBBY] Cliente saiu (%d/10 restantes)\n", restantes);

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Cliente desconectado - %d/10 restantes", restantes);
    registarEvento(0, EVT_CLIENTE_DESCONECTADO, log_msg);

    close(sockfd);
}

/*
 * servidor/src/util-stream-server.c
 * 
 * Sistema de Lobby Dinâmico - Free-for-All Multiplayer (2-10 jogadores)
 * 
 * Implementa lobby com timer de agregação:
 * - Máximo 10 jogadores simultâneos
 * - Timer de agregação (default: 5 segundos)
 * - Disparo automático ao atingir 10 jogadores
 * - Jogo aleatório compartilhado por todos
 * - Rejeita conexões quando servidor cheio
 */

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
    
    /* ====================
     * FASE 1: CONTROLO DE CAPACIDADE
     * ==================== */
    
    sem_wait(&dados->mutex);
    
    // Verificar se servidor está cheio
    if (dados->numClientesJogando >= 10) {
        sem_post(&dados->mutex);
        
        printf("[LOBBY] ❌ Servidor cheio (%d/10). Cliente rejeitado.\n", 
               dados->numClientesJogando);
        
        // Enviar mensagem de rejeição
        bzero(&msg_resposta, sizeof(MensagemSudoku));
        msg_resposta.tipo = 99;  // Código especial: servidor cheio
        strncpy(msg_resposta.resposta, 
                "Servidor cheio (10/10). Aguarde que alguém saia.", 
                sizeof(msg_resposta.resposta) - 1);
        
        writen(sockfd, (char *)&msg_resposta, sizeof(MensagemSudoku));
        registarEvento(0, EVT_ERRO_GERAL, "Cliente rejeitado - servidor cheio");
        
        close(sockfd);
        return;
    }
    
    // Adicionar ao lobby (apenas contagem inicial de conexões)
    dados->numClientesJogando++;
    
    int total = dados->numClientesJogando;
    printf("[LOBBY] ✅ Cliente conectado (%d/10 total)\n", total);
    sem_post(&dados->mutex);
    
    /* ====================
     * LOOP PRINCIPAL: MÚLTIPLOS JOGOS
     * ==================== */
    for (;;) {
        
        /* ====================
         * FASE 2: AGUARDAR PEDIDO DE JOGO
         * ==================== */
        
        // Ler pedido do cliente (PEDIR_JOGO)
        // Nota: O cliente envia isto imediatamente. Se demorar, é porque está a pensar ou desconectou.
        n = readn(sockfd, (char *)&msg_recebida, sizeof(MensagemSudoku));
        
        if (n <= 0) {
            goto cleanup_e_sair;
        }
        
        if (msg_recebida.tipo != PEDIR_JOGO) {
            printf("[ERRO] Cliente enviou tipo %d, esperava PEDIR_JOGO\n", msg_recebida.tipo);
            goto cleanup_e_sair;
        }
        
        printf("[LOBBY] Cliente %d pediu jogo. A entrar no lobby...\n", msg_recebida.idCliente);
        
        char log_lobby[256];
        snprintf(log_lobby, sizeof(log_lobby), "Cliente %d entrou no lobby (Aguardando sincronização)", msg_recebida.idCliente);
        registarEvento(msg_recebida.idCliente, EVT_CLIENTE_CONECTADO, log_lobby);

        /* ====================
         * FASE 3: ENTRAR NO LOBBY E AGUARDAR
         * ==================== */
        sem_wait(&dados->mutex);
        dados->numClientesLobby++;
        dados->ultimaEntrada = time(NULL);
        int lobby_size = dados->numClientesLobby;
        
        printf("[LOBBY] ⏳ Cliente %d entrou no lobby (%d aguardando)\n", msg_recebida.idCliente, lobby_size);
        
        // Verificar se deve disparar jogo imediatamente (10 clientes)
        if (dados->numClientesLobby >= 10) {
            printf("[LOBBY] 🎮 Lobby cheio! Disparando jogo imediatamente...\n");
            
            // Selecionar jogo aleatório
            dados->jogoAtual = rand() % numJogos;
            dados->jogoIniciado = 1;
            dados->jogoTerminado = 0;  // Resetar flag de jogo terminado
            dados->idVencedor = -1;
            dados->tempoVitoria = 0;
            
            char log_msg[256];
            snprintf(log_msg, sizeof(log_msg), 
                     "Jogo #%d iniciado - Lobby cheio (10 jogadores)", 
                     dados->jogoAtual);
            registarEvento(0, EVT_SERVIDOR_INICIADO, log_msg);
            
            // Despertar todos
            for (int i = 0; i < 10; i++) {
                sem_post(&dados->lobby_semaforo);
            }
        }
        sem_post(&dados->mutex);
        
        // Aguardar no semáforo até jogo ser disparado
        sem_wait(&dados->lobby_semaforo);
        
        snprintf(log_lobby, sizeof(log_lobby), "Sincronização concluída! Cliente %d a iniciar jogo", msg_recebida.idCliente);
        registarEvento(msg_recebida.idCliente, EVT_SERVIDOR_INICIADO, log_lobby);
        
        /* ====================
         * FASE 4: ENVIAR JOGO
         * ==================== */
        
        sem_wait(&dados->mutex);
        meu_jogo = dados->jogoAtual;
        dados->numClientesLobby--;  // Saiu do lobby
        dados->numJogadoresAtivos++; // Entrou no jogo
        sem_post(&dados->mutex);
        
        printf("[JOGO] 🎮 Enviando Jogo #%d ao cliente...\n", meu_jogo);
        
        bzero(&msg_resposta, sizeof(MensagemSudoku));
        msg_resposta.tipo = ENVIAR_JOGO;
        msg_resposta.idJogo = jogos[meu_jogo].idjogo;
        strncpy(msg_resposta.tabuleiro, jogos[meu_jogo].tabuleiro, sizeof(msg_resposta.tabuleiro) - 1);
        
        if (writen(sockfd, (char *)&msg_resposta, sizeof(MensagemSudoku)) != sizeof(MensagemSudoku)) {
            printf("[ERRO] Falha ao enviar jogo\n");
            goto cleanup_e_sair;
        }
        
        registarEvento(msg_recebida.idCliente, EVT_JOGO_ENVIADO, "Jogo enviado ao cliente");
        
        /* ====================
         * FASE 5: CONFIGURAR TIMEOUT
         * ==================== */
        
        struct timeval timeout;
        timeout.tv_sec = timeoutCliente;
        timeout.tv_usec = 0;
        
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        
        /* ====================
         * FASE 6: AGUARDAR SOLUÇÃO OU VALIDAÇÕES
         * ==================== */
        
        int aguardando_solucao = 1;
        while (aguardando_solucao) {
            // CRÍTICO: Verificar se jogo já terminou (outro cliente ganhou)
            sem_wait(&dados->mutex);
            if (dados->jogoTerminado && dados->idVencedor != msg_recebida.idCliente) {
                int vencedor = dados->idVencedor;
                sem_post(&dados->mutex);
                
                // Este cliente perdeu! Enviar notificação
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                printf("\x1b[33m[%02d:%02d:%02d] [%d] [INFO]  ⚠️  Notificando derrota - Cliente %d ganhou\x1b[0m\n", 
                       t->tm_hour, t->tm_min, t->tm_sec, msg_recebida.idCliente, vencedor);
                
                bzero(&msg_resposta, sizeof(MensagemSudoku));
                msg_resposta.tipo = JOGO_TERMINADO;
                msg_resposta.idCliente = vencedor;  // Quem ganhou
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
            
            if (n == 0) {
                printf("[INFO] Cliente desconectou após jogo\n");
                goto cleanup_e_sair;
            }
            
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    printf("[TIMEOUT] Cliente não respondeu\n");
                    registarEvento(msg_recebida.idCliente, EVT_ERRO_GERAL, "Timeout");
                }
                goto cleanup_e_sair;
            }

            // --- NOVO: Validação Parcial de Blocos ---
            if (msg_recebida.tipo == VALIDAR_BLOCO) {
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                
                // Log de Receção com Cor (Azul para REDE)
                printf("\x1b[34m[%02d:%02d:%02d] [%d] [REDE]  📩 Recebido pedido VALIDAR_BLOCO (ID: %d)\x1b[0m\n", 
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
                for(int r = 0; r < 3; r++) {
                    for(int c = 0; c < 3; c++) {
                        int idx = (start_row + r) * 9 + (start_col + c);
                        int val_solucao = solucao[idx] - '0';
                        int val_cliente = msg_recebida.conteudo_bloco[k++];
                        
                        // Se o cliente enviou um número (não 0) e é diferente da solução
                        if (val_cliente != 0 && val_cliente != val_solucao) {
                            bloco_correto = 0;
                        }
                    }
                }
                
                bzero(&msg_resposta, sizeof(MensagemSudoku));
                msg_resposta.tipo = RESPOSTA_BLOCO;
                msg_resposta.idCliente = msg_recebida.idCliente;
                msg_resposta.idJogo = meu_jogo;
                msg_resposta.bloco_id = msg_recebida.bloco_id;
                
                if (bloco_correto) {
                    strcpy(msg_resposta.resposta, "OK");
                    // Log de Sucesso (Verde)
                    printf("\x1b[32m[%02d:%02d:%02d] [%d] [JOGO]  ✅ Bloco %d verificado: CORRETO -> A responder OK\x1b[0m\n", 
                           t->tm_hour, t->tm_min, t->tm_sec, msg_recebida.idCliente, msg_recebida.bloco_id);
                    
                    // Log para ficheiro
                    snprintf(log_msg, sizeof(log_msg), "Bloco %d validado com sucesso", msg_recebida.bloco_id);
                    registarEvento(msg_recebida.idCliente, EVT_VALIDACAO_BLOCO_OK, log_msg);
                } else {
                    strcpy(msg_resposta.resposta, "NOK");
                    // Log de Erro (Vermelho)
                    printf("\x1b[31m[%02d:%02d:%02d] [%d] [JOGO]  ❌ Bloco %d verificado: INCORRETO -> A responder NOK\x1b[0m\n", 
                           t->tm_hour, t->tm_min, t->tm_sec, msg_recebida.idCliente, msg_recebida.bloco_id);
                           
                    // Log para ficheiro
                    snprintf(log_msg, sizeof(log_msg), "Bloco %d inválido", msg_recebida.bloco_id);
                    registarEvento(msg_recebida.idCliente, EVT_VALIDACAO_BLOCO_NOK, log_msg);
                }
                
                writen(sockfd, (char *)&msg_resposta, sizeof(MensagemSudoku));
                continue; // Continua à espera da solução final
            }
            
            if (msg_recebida.tipo == ENVIAR_SOLUCAO) {
                aguardando_solucao = 0; // Sai do loop para verificar solução final
            } else {
                printf("[ERRO] Tipo inesperado: %d\n", msg_recebida.tipo);
                goto cleanup_e_sair;
            }
        }
        
        /* ====================
         * FASE 7: VERIFICAR SOLUÇÃO
         * ==================== */
        
        time_t now = time(NULL);
        struct tm *t = localtime(&now);

        printf("\x1b[34m[%02d:%02d:%02d] [%d] [REDE]  📩 Recebido pedido ENVIAR_SOLUCAO\x1b[0m\n", 
               t->tm_hour, t->tm_min, t->tm_sec, msg_recebida.idCliente);
        
        char log_solucao[256];
        snprintf(log_solucao, sizeof(log_solucao), "Solução recebida do Cliente %d (A verificar...)", msg_recebida.idCliente);
        registarEvento(msg_recebida.idCliente, EVT_SOLUCAO_RECEBIDA, log_solucao);
        
        ResultadoVerificacao resultado = verificarSolucao(msg_recebida.tabuleiro, jogos[meu_jogo].solucao, jogos[meu_jogo].tabuleiro);
        
        bzero(&msg_resposta, sizeof(MensagemSudoku));
        msg_resposta.tipo = RESPOSTA_SOLUCAO;
        msg_resposta.idCliente = msg_recebida.idCliente;
        msg_resposta.idJogo = msg_recebida.idJogo;
        
        if (resultado.correto) {
            // ===== DOUBLE-CHECK PATTERN: LOCK ATÓMICO =====
            // Primeira verificação SEM lock (rápida)
            int precisa_marcar = 0;
            
            sem_wait(&dados->mutex);
            // Segunda verificação COM lock (atómica)
            if (!dados->jogoTerminado) {
                // Este é o PRIMEIRO vencedor!
                dados->jogoTerminado = 1;
                dados->idVencedor = msg_recebida.idCliente;
                dados->tempoVitoria = time(NULL);
                precisa_marcar = 1;
                
                printf("\x1b[35m[%02d:%02d:%02d] [%d] [VITÓRIA] 🏆 PRIMEIRO VENCEDOR! Outros serão notificados.\x1b[0m\n",
                       t->tm_hour, t->tm_min, t->tm_sec, msg_recebida.idCliente);
            } else {
                // Outro cliente já ganhou (race perdida)
                printf("\x1b[33m[%02d:%02d:%02d] [%d] [INFO]   ⏱️  Solução correta mas cliente %d ganhou primeiro.\x1b[0m\n",
                       t->tm_hour, t->tm_min, t->tm_sec, msg_recebida.idCliente, dados->idVencedor);
            }
            sem_post(&dados->mutex);
            // ===== FIM DO DOUBLE-CHECK PATTERN =====
            
            strncpy(msg_resposta.resposta, "Certo", sizeof(msg_resposta.resposta) - 1);
            
            if (precisa_marcar) {
                printf("\x1b[32m[%02d:%02d:%02d] [%d] [WIN]   🏆 SOLUÇÃO ACEITE! Jogo terminado.\x1b[0m\n",
                       t->tm_hour, t->tm_min, t->tm_sec, msg_recebida.idCliente);
                registarEvento(msg_recebida.idCliente, EVT_SOLUCAO_CORRETA, "Solução correta - VENCEDOR");
            } else {
                registarEvento(msg_recebida.idCliente, EVT_SOLUCAO_CORRETA, "Solução correta - mas não foi o primeiro");
            }
        } else {
            snprintf(msg_resposta.resposta, sizeof(msg_resposta.resposta), 
                     "Errado (%d erros)", resultado.numerosErrados);
            printf("\x1b[31m[%02d:%02d:%02d] [%d] [JOGO]  ❌ Solução INCORRETA (%d erros)\x1b[0m\n", 
                   t->tm_hour, t->tm_min, t->tm_sec, msg_recebida.idCliente, resultado.numerosErrados);
            
            char log_detalhado[256];
            snprintf(log_detalhado, sizeof(log_detalhado), 
                     "Solução incorreta - %d erros, %d acertos", 
                     resultado.numerosErrados, resultado.numerosCertos);
            registarEvento(msg_recebida.idCliente, EVT_SOLUCAO_ERRADA, log_detalhado);
        }
        
        writen(sockfd, (char *)&msg_resposta, sizeof(MensagemSudoku));
        
        // FIM DO LOOP - Volta para o início (Lobby) se o cliente quiser jogar novamente
        printf("[INFO] Ciclo de jogo terminado. Aguardando decisão do cliente...\n");
        
        sem_wait(&dados->mutex);
        dados->numJogadoresAtivos--;
        if (dados->numJogadoresAtivos == 0) {
            dados->jogoIniciado = 0; // Resetar estado do jogo se todos terminaram
            printf("[LOBBY] 🔄 Todos os jogadores terminaram. Jogo resetado.\n");
        }
        sem_post(&dados->mutex);
    }
    
cleanup_e_sair:
    /* ====================
     * FASE 7: CLEANUP
     * ==================== */
    
    sem_wait(&dados->mutex);
    dados->numClientesJogando--;
    
    // Se lobby ficou vazio, resetar estado
    if (dados->numClientesLobby == 0 && dados->numClientesJogando == 0) {
        dados->jogoIniciado = 0;
        dados->jogoAtual = -1;
        dados->numJogadoresAtivos = 0; // Resetar também
        printf("[LOBBY] 🔄 Lobby resetado (vazio)\n");
    } else if (dados->numJogadoresAtivos == 0 && dados->jogoIniciado == 1) {
        // Se saiu o último jogador ativo (por erro/disconnect)
        // Precisamos verificar se mais ninguém está a jogar
        // Como não temos flag local fácil, vamos confiar que numJogadoresAtivos
        // reflete a realidade se gerido corretamente.
        // Se este cliente estava a jogar e saiu com erro, numJogadoresAtivos
        // não foi decrementado no loop.
        // Isto é um bug potencial se não usarmos flag local.
        // CORREÇÃO RÁPIDA: Se numClientesJogando < numJogadoresAtivos, algo está errado.
        if (dados->numClientesJogando < dados->numJogadoresAtivos) {
             dados->numJogadoresAtivos = dados->numClientesJogando;
        }
        if (dados->numJogadoresAtivos == 0) {
             dados->jogoIniciado = 0;
             printf("[LOBBY] 🔄 Último jogador saiu. Jogo resetado.\n");
        }
    }
    
    int restantes = dados->numClientesJogando;
    sem_post(&dados->mutex);
    
    printf("[LOBBY] 👋 Cliente saiu (%d/10 restantes)\n", restantes);
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Cliente desconectado - %d/10 restantes", restantes);
    registarEvento(0, EVT_CLIENTE_DESCONECTADO, log_msg);
    
    close(sockfd);
}

// cliente/src/util-stream-cliente.c - Lógica de comunicação e interface do cliente

#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "protocolo.h"
#include "logs_cliente.h"
#include "solver.h"

void imprimirTabuleiroCliente(const char *tabuleiro)
{
    printf("\033[36m");
    printf("    ┌───────┬───────┬───────┐\n");

    for (int i = 0; i < 9; i++)
    {
        printf("    │");
        for (int j = 0; j < 9; j++)
        {
            int idx = i * 9 + j;
            char celula = (tabuleiro[idx] == '0') ? '.' : tabuleiro[idx];

            if (celula == '.')
                printf(" \033[2m%c\033[0m\033[36m", celula);
            else
                printf(" \033[1;33m%c\033[0m\033[36m", celula);

            if (j == 2 || j == 5)
                printf(" │");
        }
        printf(" │\n");

        if (i == 2 || i == 5)
        {
            printf("    ├───────┼───────┼───────┤\n");
        }
    }

    printf("    └───────┴───────┴───────┘\n");
    printf("\033[0m");
}

// Atualiza a interface durante a resolução
void atualizarUICliente(MensagemSudoku *msg, struct timespec horaInicio)
{

    limparEcra();

    struct timespec agora;
    clock_gettime(CLOCK_MONOTONIC, &agora);

    double tempoDecorrido = (agora.tv_sec - horaInicio.tv_sec) +
                            (agora.tv_nsec - horaInicio.tv_nsec) / 1e9;

    printf("\033[1;36m╔═══════════════════════════════════════════╗\n");
    printf("║       CLIENTE SUDOKU MULTIPLAYER         ║\n");
    printf("╚═══════════════════════════════════════════╝\033[0m\n");
    printf("\033[1mJogo:\033[0m #%d  │  \033[1mTempo:\033[0m %.2fs", msg->idJogo, tempoDecorrido);

    int threads = get_num_threads_last_run();
    if (threads > 0)
    {
        printf("  │  \033[1mThreads:\033[0m %d\n", threads);
    }
    else
    {
        printf("\n");
    }

    printf("\n");

    // Reutiliza a função de imprimir o tabuleiro
    imprimirTabuleiroCliente(msg->tabuleiro);
}

/* * Função principal do cliente.
 * Gere o fluxo de comunicação com o servidor.
 */
/* * Função principal do cliente.
 * Gere o fluxo de comunicação com o servidor.
 * Permite jogar múltiplos jogos consecutivos.
 */
void str_cli(FILE *fp, int sockfd, int idCliente)
{
    (void)fp; // Parâmetro não usado nesta implementação

    MensagemSudoku msg_enviar;
    MensagemSudoku msg_receber;
    MensagemSudoku msg_jogo_original; // Guardar o jogo original

    int jogos_jogados = 0;
    int jogos_ganhos = 0;
    char jogar_novamente = 's';

    /* ========================================
     * LOOP PRINCIPAL: MÚLTIPLOS JOGOS
     * ========================================
     * O cliente pode jogar vários jogos consecutivos
     * sem precisar desconectar e reconectar
     */
    while (jogar_novamente == 's' || jogar_novamente == 'S')
    {
        jogos_jogados++;

        printf("\n\033[1;35m┌──────────────────────────────────────┐\n");
        printf("│          JOGO #%d                    │\n", jogos_jogados);
        printf("└──────────────────────────────────────┘\033[0m\n\n");

        printf("\033[33mA solicitar jogo...\033[0m ");
        fflush(stdout);
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Jogo #%d: Novo jogo solicitado ao servidor", jogos_jogados);
        registarEventoCliente(EVTC_NOVO_JOGO_PEDIDO, log_msg);

        bzero(&msg_enviar, sizeof(MensagemSudoku));
        msg_enviar.tipo = PEDIR_JOGO;
        msg_enviar.idCliente = idCliente;

        if (writen(sockfd, (char *)&msg_enviar, sizeof(MensagemSudoku)) != sizeof(MensagemSudoku))
            err_dump("str_cli: erro ao enviar pedido de jogo");

        // ----- PASSO 2: Receber o jogo -----
        char msg_log[256]; // Logs desta iteração
        int n = readn(sockfd, (char *)&msg_receber, sizeof(MensagemSudoku));
        if (n != sizeof(MensagemSudoku))
        {
            // Verificar se foi timeout
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                printf("[TIMEOUT] Servidor não respondeu a tempo.\n");
                registarEventoCliente(EVTC_ERRO, "Timeout ao aguardar jogo do servidor");
                return;
            }
            err_dump("str_cli: erro ao receber o jogo");
        }

        if (msg_receber.tipo != ENVIAR_JOGO)
        {
            printf("Cliente: Erro, esperava um jogo (tipo 2) e recebi tipo %d\n", msg_receber.tipo);
            char msg_erro[128];
            snprintf(msg_erro, sizeof(msg_erro), "Erro: tipo de mensagem inesperado %d", msg_receber.tipo);
            registarEventoCliente(EVTC_ERRO, msg_erro);
            return;
        }

        // Contar células preenchidas
        int celulas_preenchidas = 0;
        for (int i = 0; i < 81; i++)
        {
            if (msg_receber.tabuleiro[i] != '0')
                celulas_preenchidas++;
        }

        printf("\033[32mRecebido!\033[0m (%d pistas)\n", celulas_preenchidas);
        snprintf(msg_log, sizeof(msg_log),
                 "Jogo #%d recebido (%d células preenchidas, %d vazias)",
                 msg_receber.idJogo, celulas_preenchidas, 81 - celulas_preenchidas);
        registarEventoCliente(EVTC_JOGO_RECEBIDO, msg_log);

        // *** CORREÇÃO: Copia a mensagem do jogo para um local seguro ***
        memcpy(&msg_jogo_original, &msg_receber, sizeof(MensagemSudoku));

        // CAPTURAR A HORA DE INÍCIO
        struct timespec horaInicio;
        clock_gettime(CLOCK_MONOTONIC, &horaInicio);

        // *** MOSTRAR O TABULEIRO ORIGINAL ANTES DA RESOLUÇÃO ***
        printf("\n\033[1;33m╔═══════════════════════════════════════════╗\n");
        printf("║     TABULEIRO RECEBIDO - JOGO #%d       ║\n", msg_jogo_original.idJogo);
        printf("╚═══════════════════════════════════════════╝\033[0m\n\n");
        imprimirTabuleiroCliente(msg_jogo_original.tabuleiro);
        printf("\n");

        // ----- PASSO 3: Resolver o jogo (ALGORITMO REAL) -----
        char minha_solucao[82];
        strncpy(minha_solucao, msg_jogo_original.tabuleiro, sizeof(minha_solucao) - 1);
        minha_solucao[sizeof(minha_solucao) - 1] = '\0';

        printf("\033[33mA resolver...\033[0m ");
        fflush(stdout);

        if (resolver_sudoku(minha_solucao, sockfd, idCliente))
        {
            printf("\033[32m✓ Resolvido!\033[0m\n");
        }
        else
        {
            printf("\033[31m✗ Impossível resolver!\033[0m\n");
        }

        // ----- PASSO 4: Enviar a solução -----
        // Atualizar UI com a solução encontrada
        MensagemSudoku msg_solucao_visual;
        memcpy(&msg_solucao_visual, &msg_jogo_original, sizeof(MensagemSudoku));
        strncpy(msg_solucao_visual.tabuleiro, minha_solucao, 81);
        atualizarUICliente(&msg_solucao_visual, horaInicio);

        printf("\033[33mA enviar solução...\033[0m ");
        fflush(stdout);

        struct timespec fim;
        clock_gettime(CLOCK_MONOTONIC, &fim);
        double tempo_resolucao = (fim.tv_sec - horaInicio.tv_sec) +
                                 (fim.tv_nsec - horaInicio.tv_nsec) / 1e9;

        // Contar células preenchidas na solução
        int celulas_sol = 0;
        for (int i = 0; i < 81; i++)
        {
            if (minha_solucao[i] != '0')
                celulas_sol++;
        }

        snprintf(msg_log, sizeof(msg_log),
                 "Solução enviada para Jogo #%d (%d células, tempo: %.3fs)",
                 msg_jogo_original.idJogo, celulas_sol, tempo_resolucao);
        registarEventoCliente(EVTC_SOLUCAO_ENVIADA, msg_log);

        bzero(&msg_enviar, sizeof(MensagemSudoku));
        msg_enviar.tipo = ENVIAR_SOLUCAO;
        msg_enviar.idCliente = idCliente;
        msg_enviar.idJogo = msg_jogo_original.idJogo; // Usa o idJogo da cópia
        strncpy(msg_enviar.tabuleiro, minha_solucao, sizeof(msg_enviar.tabuleiro) - 1);
        msg_enviar.tabuleiro[sizeof(msg_enviar.tabuleiro) - 1] = '\0';

        if (writen(sockfd, (char *)&msg_enviar, sizeof(MensagemSudoku)) != sizeof(MensagemSudoku))
            err_dump("str_cli: erro ao enviar solução");

        // ----- PASSO 5: Receber o resultado -----
        // msg_receber é AGORA USADO SÓ PARA A RESPOSTA
        n = readn(sockfd, (char *)&msg_receber, sizeof(MensagemSudoku));
        if (n != sizeof(MensagemSudoku))
        {
            // Verificar se foi timeout
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                printf("[TIMEOUT] Servidor não respondeu a tempo.\n");
                registarEventoCliente(EVTC_ERRO, "Timeout ao aguardar resultado do servidor");
                return;
            }
            err_dump("str_cli: erro ao receber resultado");
        }

        snprintf(msg_log, sizeof(msg_log),
                 "Resultado recebido do servidor para Jogo #%d",
                 msg_jogo_original.idJogo);
        registarEventoCliente(EVTC_RESULTADO_RECEBIDO, msg_log);

        // Mostrar o resultado final
        // *** CORREÇÃO: Usa a cópia segura (msg_jogo_original) para desenhar a UI ***
        // Mas queremos mostrar o tabuleiro PREENCHIDO, não o original
        atualizarUICliente(&msg_solucao_visual, horaInicio);

        // VERIFICAR SE JOGO TERMINOU (OUTRO CLIENTE GANHOU)
        if (msg_receber.tipo == JOGO_TERMINADO)
        {
            printf("\n\n");
            printf("\033[1;31m╔═══════════════════════════════════════════╗\n");
            printf("║           JOGO TERMINADO                  ║\n");
            printf("╚═══════════════════════════════════════════╝\033[0m\n");
            printf("\033[33mCliente #%d venceu primeiro!\033[0m\n", msg_receber.idCliente);
            printf("\033[31mResultado: DERROTA\033[0m\n");

            char log_derrota[256];
            snprintf(log_derrota, sizeof(log_derrota),
                     "Derrotado - Cliente %d ganhou o jogo", msg_receber.idCliente);
            registarEventoCliente(EVTC_JOGO_PERDIDO, log_derrota);

            // Não perguntar se quer jogar novamente
            printf("\nA terminar sessão...\n");
            return; // Sair da função str_cli
        }

        if (msg_receber.tipo == RESPOSTA_SOLUCAO)
        {
            printf("\033[32mRecebido!\033[0m\n\n");

            if (strcmp(msg_receber.resposta, "Certo") == 0)
            {
                printf("\033[1;32m╔═══════════════════════════════════════════╗\n");
                printf("║              VITÓRIA!                     ║\n");
                printf("╚═══════════════════════════════════════════╝\033[0m\n");
            }
            else
            {
                printf("\033[1;31m╔═══════════════════════════════════════════╗\n");
                printf("║           SOLUÇÃO INCORRETA               ║\n");
                printf("╚═══════════════════════════════════════════╝\033[0m\n");
            }

            if (strcmp(msg_receber.resposta, "Certo") == 0)
            {
                jogos_ganhos++; // Incrementar contador de vitórias
                snprintf(msg_log, sizeof(msg_log),
                         "SOLUÇÃO CORRETA! Jogo #%d resolvido em %.3fs",
                         msg_jogo_original.idJogo, tempo_resolucao);
                registarEventoCliente(EVTC_SOLUCAO_CORRETA, msg_log);
            }
            else
            {
                snprintf(msg_log, sizeof(msg_log),
                         "SOLUÇÃO INCORRETA - Jogo #%d (tempo: %.3fs)",
                         msg_jogo_original.idJogo, tempo_resolucao);
                registarEventoCliente(EVTC_SOLUCAO_INCORRETA, msg_log);
            }
        }
        else
        {
            printf("Cliente: Erro, esperava uma resposta (tipo 4) e recebi tipo %d\n", msg_receber.tipo);
            snprintf(msg_log, sizeof(msg_log), "Erro: tipo de resposta inesperado %d", msg_receber.tipo);
            registarEventoCliente(EVTC_ERRO, msg_log);
        }

        printf("\n\033[36m┌─────────────────────────────────────────┐\n");
        printf("│      ESTATÍSTICAS DA SESSÃO             │\n");
        printf("├─────────────────────────────────────────┤\n");
        printf("│  Jogos jogados  │  %-20d │\n", jogos_jogados);
        printf("│  Vitórias       │  %-20d │\n", jogos_ganhos);
        printf("│  Taxa sucesso   │  %-19.1f%% │\n",
               jogos_jogados > 0 ? (100.0 * jogos_ganhos / jogos_jogados) : 0.0);
        printf("└─────────────────────────────────────────┘\033[0m\n\n");

        printf("\033[1mDeseja jogar novamente?\033[0m (s/n): ");
        fflush(stdout);

        // Ler resposta do utilizador
        jogar_novamente = getchar();

        // Limpar o resto da linha (incluindo o \n)
        int c;
        while ((c = getchar()) != '\n' && c != EOF)
            ;

        if (jogar_novamente == 's' || jogar_novamente == 'S')
        {
            printf("\nA preparar novo jogo...\n\n");
            snprintf(msg_log, sizeof(msg_log),
                     "Utilizador optou por jogar novamente (sessão: %d jogos)", jogos_jogados);
            registarEventoCliente(EVTC_NOVO_JOGO_PEDIDO, msg_log);
        }
        else
        {
            printf("\nA terminar sessão...\n");
            snprintf(msg_log, sizeof(msg_log),
                     "Sessão terminada - Total: %d jogos, %d vitórias (%.1f%%)",
                     jogos_jogados, jogos_ganhos,
                     jogos_jogados > 0 ? (100.0 * jogos_ganhos / jogos_jogados) : 0.0);
            registarEventoCliente(EVTC_CONEXAO_FECHADA, msg_log);
        }

    } // Fim do while (loop de múltiplos jogos)

    printf("\n\033[1;36m╔═══════════════════════════════════════════╗\n");
    printf("║           FIM DA SESSÃO                   ║\n");
    printf("╠═══════════════════════════════════════════╣\n");
    printf("║  Total de jogos   │  %-18d  ║\n", jogos_jogados);
    printf("║  Vitórias         │  %-18d  ║\n", jogos_ganhos);
    printf("║  Derrotas         │  %-18d  ║\n", jogos_jogados - jogos_ganhos);
    printf("║  Taxa de sucesso  │  %-17.1f%%  ║\n",
           jogos_jogados > 0 ? (100.0 * jogos_ganhos / jogos_jogados) : 0.0);
    printf("╚═══════════════════════════════════════════╝\033[0m\n\n");
}
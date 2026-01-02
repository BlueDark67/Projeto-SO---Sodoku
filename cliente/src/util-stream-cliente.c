/*
 * cliente/src/util-stream-cliente.c
 * 
 * Lógica de Processamento e Interface do Cliente
 * 
 * Este módulo implementa:
 * - Comunicação com o servidor (pedidos e respostas)
 * - Simulação de resolução de jogos Sudoku
 * - Interface de utilizador atualizável em tempo real
 * - Apresentação visual dos tabuleiros
 * - Temporizador de resolução
 * - Registo de eventos no log do cliente
 * 
 * Protocolo de comunicação:
 * 1. Cliente envia PEDIR_JOGO
 * 2. Servidor responde com ENVIAR_JOGO
 * 3. Cliente "resolve" e envia ENVIAR_SOLUCAO
 * 4. Servidor verifica e envia RESULTADO
 */

#include "util.h"
#include <string.h>
#include <stdlib.h> 
#include <time.h>   // Temporizador de resolução
#include <unistd.h> // sleep() para animação
#include <errno.h>  // Para EAGAIN, EWOULDBLOCK

// Headers do projeto
#include "protocolo.h"      // Tipos de mensagens
#include "logs_cliente.h"   // Sistema de logging
#include "solver.h"         // Algoritmo de resolução (Backtracking)

/**
 * @brief Imprime um tabuleiro de forma visual no terminal do cliente.
 */
void imprimirTabuleiroCliente(const char *tabuleiro)
{
    for (int i = 0; i < 9; i++)
    {
        for (int j = 0; j < 9; j++)
        {
            int idx = i * 9 + j;
            char celula = (tabuleiro[idx] == '0') ? '.' : tabuleiro[idx]; // Mostra '.' para vazios
            printf(" %c ", celula);
            if (j == 2 || j == 5)
                printf("|");
        }
        printf("\n");
        if (i == 2 || i == 5)
        {
            printf("---------+---------+---------\n");
        }
    }
}

/**
 * @brief Atualiza a interface do utilizador do cliente
 * 
 * Limpa o ecrã e redesenha toda a interface com:
 * - Informações do jogo atual (ID)
 * - Tempo decorrido desde o início
 * - Tabuleiro visual atualizado
 * 
 * @param msg Mensagem contendo o jogo a apresentar
 * @param horaInicio Timestamp do início da resolução (para calcular tempo)
 */
void atualizarUICliente(MensagemSudoku *msg, struct timespec horaInicio)
{

    // Limpa o ecrã de forma portável
    limparEcra();

    struct timespec agora;
    clock_gettime(CLOCK_MONOTONIC, &agora);
    
    double tempoDecorrido = (agora.tv_sec - horaInicio.tv_sec) + 
                           (agora.tv_nsec - horaInicio.tv_nsec) / 1e9;

    printf("===========================================\n");
    printf("        CLIENTE SUDOKU \n");
    printf("===========================================\n");
    printf(" ID Jogo a decorrer: %d\n", msg->idJogo);
    printf(" Tempo decorrido   : %.3f segundos\n", tempoDecorrido);
    
    int threads = get_num_threads_last_run();
    if (threads > 0) {
        printf(" Tarefas           : %d (Paralelo)\n", threads);
    } else {
        printf(" Tarefas           : 1 (Simulação/Seq)\n");
    }
    
    printf("-------------------------------------------\n\n");

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
    while (jogar_novamente == 's' || jogar_novamente == 'S') {
        jogos_jogados++;
        
        printf("\n===========================================\n");
        printf("   JOGO #%d\n", jogos_jogados);
        printf("===========================================\n\n");

        // ----- PASSO 1: Pedir um jogo -----
        printf("Cliente: A pedir jogo ao servidor (Meu ID: %d)...\n", idCliente);
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Jogo #%d: Novo jogo solicitado ao servidor", jogos_jogados);
        registarEventoCliente(EVTC_NOVO_JOGO_PEDIDO, log_msg);
        
        bzero(&msg_enviar, sizeof(MensagemSudoku));
        msg_enviar.tipo = PEDIR_JOGO;
        msg_enviar.idCliente = idCliente;

        if (writen(sockfd, (char *)&msg_enviar, sizeof(MensagemSudoku)) != sizeof(MensagemSudoku))
            err_dump("str_cli: erro ao enviar pedido de jogo");

        // ----- PASSO 2: Receber o jogo -----
        char msg_log[256];  // Logs desta iteração
        int n = readn(sockfd, (char *)&msg_receber, sizeof(MensagemSudoku));
        if (n != sizeof(MensagemSudoku)) {
            // Verificar se foi timeout
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
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
        for (int i = 0; i < 81; i++) {
            if (msg_receber.tabuleiro[i] != '0') celulas_preenchidas++;
        }
        
        snprintf(msg_log, sizeof(msg_log), 
                 "Jogo #%d recebido (%d células preenchidas, %d vazias)", 
                 msg_receber.idJogo, celulas_preenchidas, 81 - celulas_preenchidas);
        registarEventoCliente(EVTC_JOGO_RECEBIDO, msg_log);

        // *** CORREÇÃO: Copia a mensagem do jogo para um local seguro ***
        memcpy(&msg_jogo_original, &msg_receber, sizeof(MensagemSudoku));

        // CAPTURAR A HORA DE INÍCIO
        struct timespec horaInicio;
        clock_gettime(CLOCK_MONOTONIC, &horaInicio);

        // ----- PASSO 3: Resolver o jogo (ALGORITMO REAL) -----
        char minha_solucao[82];
        strncpy(minha_solucao, msg_jogo_original.tabuleiro, sizeof(minha_solucao) - 1);
        minha_solucao[sizeof(minha_solucao) - 1] = '\0';

        printf("\nA resolver o Sudoku (Backtracking)... ");
        fflush(stdout);
        
        // Chama o solver real (bloqueante)
        if (resolver_sudoku(minha_solucao, sockfd, idCliente)) {
            printf("Solução encontrada!\n");
        } else {
            printf("Impossível resolver este tabuleiro!\n");
            // Em caso de falha, envia o tabuleiro incompleto (o servidor dirá que está errado)
        }

        // ----- PASSO 4: Enviar a solução -----
        // Atualizar UI com a solução encontrada
        MensagemSudoku msg_solucao_visual;
        memcpy(&msg_solucao_visual, &msg_jogo_original, sizeof(MensagemSudoku));
        strncpy(msg_solucao_visual.tabuleiro, minha_solucao, 81);
        atualizarUICliente(&msg_solucao_visual, horaInicio);

        // *** CORREÇÃO: Adicionado \n no fim do printf ***
        printf("\nA enviar solução para o servidor...\n");
        
        struct timespec fim;
        clock_gettime(CLOCK_MONOTONIC, &fim);
        double tempo_resolucao = (fim.tv_sec - horaInicio.tv_sec) + 
                                (fim.tv_nsec - horaInicio.tv_nsec) / 1e9;
        
        // Contar células preenchidas na solução
        int celulas_sol = 0;
        for (int i = 0; i < 81; i++) {
            if (minha_solucao[i] != '0') celulas_sol++;
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
        if (n != sizeof(MensagemSudoku)) {
            // Verificar se foi timeout
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
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
        if (msg_receber.tipo == JOGO_TERMINADO) {
            printf("\n");
            printf("════════════════════════════════════════\n");
            printf("   ⚠️  JOGO TERMINADO\n");
            printf("════════════════════════════════════════\n");
            printf("Cliente %d encontrou a solução primeiro!\n", msg_receber.idCliente);
            printf("Resultado: DERROTA 😞\n");
            printf("════════════════════════════════════════\n");
            
            char log_derrota[256];
            snprintf(log_derrota, sizeof(log_derrota), 
                     "Derrotado - Cliente %d ganhou o jogo", msg_receber.idCliente);
            registarEventoCliente(EVTC_JOGO_PERDIDO, log_derrota);
            
            // Não perguntar se quer jogar novamente
            printf("\nA terminar sessão...\n");
            return;  // Sair da função str_cli
        }

        if (msg_receber.tipo == RESPOSTA_SOLUCAO)
        {
            printf("\n===================================\n");
                // Mas usa a resposta do msg_receber
            printf("  Resultado do Servidor: %s\n", msg_receber.resposta);
            printf("===================================\n");
            
            if (strcmp(msg_receber.resposta, "Certo") == 0) {
                jogos_ganhos++;  // Incrementar contador de vitórias
                snprintf(msg_log, sizeof(msg_log), 
                         "✓ SOLUÇÃO CORRETA! Jogo #%d resolvido em %.3fs", 
                         msg_jogo_original.idJogo, tempo_resolucao);
                registarEventoCliente(EVTC_SOLUCAO_CORRETA, msg_log);
            } else {
                snprintf(msg_log, sizeof(msg_log), 
                         "✗ SOLUÇÃO INCORRETA - Jogo #%d (tempo: %.3fs)", 
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

        /* ========================================
         * PROMPT: JOGAR NOVAMENTE?
         * ======================================== */
        printf("\n-------------------------------------------\n");
        printf("  Estatísticas da Sessão:\n");
        printf("  Jogos jogados: %d\n", jogos_jogados);
        printf("  Vitórias: %d\n", jogos_ganhos);
        printf("  Taxa de sucesso: %.1f%%\n", 
               jogos_jogados > 0 ? (100.0 * jogos_ganhos / jogos_jogados) : 0.0);
        printf("-------------------------------------------\n\n");
        
        printf("Deseja jogar novamente? (s/n): ");
        fflush(stdout);
        
        // Ler resposta do utilizador
        jogar_novamente = getchar();
        
        // Limpar o resto da linha (incluindo o \n)
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        
        if (jogar_novamente == 's' || jogar_novamente == 'S') {
            printf("\n🎮 A preparar novo jogo...\n\n");
            snprintf(msg_log, sizeof(msg_log), 
                     "Utilizador optou por jogar novamente (sessão: %d jogos)", jogos_jogados);
            registarEventoCliente(EVTC_NOVO_JOGO_PEDIDO, msg_log);
        } else {
            printf("\n👋 A terminar sessão...\n");
            snprintf(msg_log, sizeof(msg_log), 
                     "Sessão terminada - Total: %d jogos, %d vitórias (%.1f%%)", 
                     jogos_jogados, jogos_ganhos,
                     jogos_jogados > 0 ? (100.0 * jogos_ganhos / jogos_jogados) : 0.0);
            registarEventoCliente(EVTC_CONEXAO_FECHADA, msg_log);
        }
        
    } // Fim do while (loop de múltiplos jogos)
    
    // Mensagem final
    printf("\n===========================================\n");
    printf("   FIM DA SESSÃO\n");
    printf("===========================================\n");
    printf("  Total de jogos: %d\n", jogos_jogados);
    printf("  Vitórias: %d\n", jogos_ganhos);
    printf("  Derrotas: %d\n", jogos_jogados - jogos_ganhos);
    printf("  Taxa de sucesso: %.1f%%\n", 
           jogos_jogados > 0 ? (100.0 * jogos_ganhos / jogos_jogados) : 0.0);
    printf("===========================================\n\n");
}
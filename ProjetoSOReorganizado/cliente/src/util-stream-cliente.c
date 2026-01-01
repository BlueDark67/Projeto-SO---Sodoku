/*
 * util-stream-cliente.c
 * Lógica de processamento do cliente
 */

#include "util.h"
#include <string.h>
#include <stdlib.h> // Para system("clear")
#include <time.h>   // Para o temporizador
#include <unistd.h> // Para sleep()

// --- INCLUDES ADICIONADOS ---
#include "protocolo.h" // common/include
#include "logs_cliente.h" // Logs do cliente
// -----------------------------

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
 * @brief NOVA FUNÇÃO: Limpa o ecrã e redesenha a UI do cliente
 */
void atualizarUICliente(MensagemSudoku *msg, time_t horaInicio)
{

    // Limpa o ecrã (funciona em Linux/macOS)
    // Pode usar system("cls") no Windows
    system("clear");

    time_t agora = time(NULL);
    double tempoDecorrido = difftime(agora, horaInicio);

    printf("===========================================\n");
    printf("        CLIENTE SUDOKU \n");
    printf("===========================================\n");
    printf(" ID Jogo a decorrer: %d\n", msg->idJogo);
    printf(" Tempo decorrido   : %.0f segundos\n", tempoDecorrido);
    printf(" Tarefas           : 1 (Simulação)\n"); // Para a Fase 3, isto será dinâmico
    printf("-------------------------------------------\n\n");

    // Reutiliza a função de imprimir o tabuleiro
    imprimirTabuleiroCliente(msg->tabuleiro);
}

/* * Função principal do cliente.
 * Gere o fluxo de comunicação com o servidor.
 */
/* * Função principal do cliente.
 * Gere o fluxo de comunicação com o servidor.
 */
void str_cli(FILE *fp, int sockfd, int idCliente)
{
    (void)fp; // Parâmetro não usado nesta implementação
    
    MensagemSudoku msg_enviar;
    MensagemSudoku msg_receber;

    // VAMOS GUARDAR O JOGO ORIGINAL AQUI
    MensagemSudoku msg_jogo_original;

    // ----- PASSO 1: Pedir um jogo -----
    printf("Cliente: A pedir jogo ao servidor (Meu ID: %d)...\n", idCliente);
    registarEventoCliente(EVTC_NOVO_JOGO_PEDIDO, "Novo jogo solicitado ao servidor");
    
    bzero(&msg_enviar, sizeof(MensagemSudoku));
    msg_enviar.tipo = PEDIR_JOGO;
    msg_enviar.idCliente = idCliente;

    if (writen(sockfd, (char *)&msg_enviar, sizeof(MensagemSudoku)) != sizeof(MensagemSudoku))
        err_dump("str_cli: erro ao enviar pedido de jogo");

    // ----- PASSO 2: Receber o jogo -----
    if (readn(sockfd, (char *)&msg_receber, sizeof(MensagemSudoku)) != sizeof(MensagemSudoku))
        err_dump("str_cli: erro ao receber jogo");

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
    
    char msg_log[256];
    snprintf(msg_log, sizeof(msg_log), 
             "Jogo #%d recebido (%d células preenchidas, %d vazias)", 
             msg_receber.idJogo, celulas_preenchidas, 81 - celulas_preenchidas);
    registarEventoCliente(EVTC_JOGO_RECEBIDO, msg_log);

    // *** CORREÇÃO: Copia a mensagem do jogo para um local seguro ***
    memcpy(&msg_jogo_original, &msg_receber, sizeof(MensagemSudoku));

    // CAPTURAR A HORA DE INÍCIO
    time_t horaInicio = time(NULL);

    // ----- PASSO 3: Resolver o jogo (SIMULAÇÃO com UI) -----
    char minha_solucao[82];
    strcpy(minha_solucao, msg_jogo_original.tabuleiro);

    for (int i = 0; i < 5; i++)
    {
        // Usa a cópia segura para desenhar a UI
        atualizarUICliente(&msg_jogo_original, horaInicio);
        printf("\nA 'resolver' o tabuleiro (simulação %d/5)...", i + 1);
        fflush(stdout);
        sleep(1);
    }

    for (int i = 0; i < 81; i++)
    {
        if (minha_solucao[i] == '0')
        {
            minha_solucao[i] = '9';
            break;
        }
    }

    // ----- PASSO 4: Enviar a solução -----
    atualizarUICliente(&msg_jogo_original, horaInicio);

    // *** CORREÇÃO: Adicionado \n no fim do printf ***
    printf("\nA enviar solução para o servidor...\n");
    
    time_t fim = time(NULL);
    double tempo_resolucao = difftime(fim, horaInicio);
    
    // Contar células preenchidas na solução
    int celulas_sol = 0;
    for (int i = 0; i < 81; i++) {
        if (minha_solucao[i] != '0') celulas_sol++;
    }
    
    snprintf(msg_log, sizeof(msg_log), 
             "Solução enviada para Jogo #%d (%d células, tempo: %.0fs)", 
             msg_jogo_original.idJogo, celulas_sol, tempo_resolucao);
    registarEventoCliente(EVTC_SOLUCAO_ENVIADA, msg_log);

    bzero(&msg_enviar, sizeof(MensagemSudoku));
    msg_enviar.tipo = ENVIAR_SOLUCAO;
    msg_enviar.idCliente = idCliente;
    msg_enviar.idJogo = msg_jogo_original.idJogo; // Usa o idJogo da cópia
    strcpy(msg_enviar.tabuleiro, minha_solucao);

    if (writen(sockfd, (char *)&msg_enviar, sizeof(MensagemSudoku)) != sizeof(MensagemSudoku))
        err_dump("str_cli: erro ao enviar solução");

    // ----- PASSO 5: Receber o resultado -----
    // msg_receber é AGORA USADO SÓ PARA A RESPOSTA
    if (readn(sockfd, (char *)&msg_receber, sizeof(MensagemSudoku)) != sizeof(MensagemSudoku))
        err_dump("str_cli: erro ao receber resultado");
    
    snprintf(msg_log, sizeof(msg_log), 
             "Resultado recebido do servidor para Jogo #%d", 
             msg_jogo_original.idJogo);
    registarEventoCliente(EVTC_RESULTADO_RECEBIDO, msg_log);

    // Mostrar o resultado final
    // *** CORREÇÃO: Usa a cópia segura (msg_jogo_original) para desenhar a UI ***
    atualizarUICliente(&msg_jogo_original, horaInicio);

    if (msg_receber.tipo == RESPOSTA_SOLUCAO)
    {
        printf("\n===================================\n");
        // Mas usa a resposta do msg_receber
        printf("  Resultado do Servidor: %s\n", msg_receber.resposta);
        printf("===================================\n");
        
        if (strcmp(msg_receber.resposta, "Certo") == 0) {
            snprintf(msg_log, sizeof(msg_log), 
                     "✓ SOLUÇÃO CORRETA! Jogo #%d resolvido em %.0fs", 
                     msg_jogo_original.idJogo, tempo_resolucao);
            registarEventoCliente(EVTC_SOLUCAO_CORRETA, msg_log);
        } else {
            snprintf(msg_log, sizeof(msg_log), 
                     "✗ SOLUÇÃO INCORRETA - Jogo #%d (tempo: %.0fs)", 
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

    printf("Pressione ENTER para fechar...\n");
    getchar();
}
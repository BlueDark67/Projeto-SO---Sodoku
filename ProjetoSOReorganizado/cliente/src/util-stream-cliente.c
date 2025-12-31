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
    MensagemSudoku msg_enviar;
    MensagemSudoku msg_receber;

    // VAMOS GUARDAR O JOGO ORIGINAL AQUI
    MensagemSudoku msg_jogo_original;

    // ----- PASSO 1: Pedir um jogo -----
    printf("Cliente: A pedir jogo ao servidor (Meu ID: %d)...\n", idCliente);
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
        return;
    }

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

    // Mostrar o resultado final
    // *** CORREÇÃO: Usa a cópia segura (msg_jogo_original) para desenhar a UI ***
    atualizarUICliente(&msg_jogo_original, horaInicio);

    if (msg_receber.tipo == RESPOSTA_SOLUCAO)
    {
        printf("\n===================================\n");
        // Mas usa a resposta do msg_receber
        printf("  Resultado do Servidor: %s\n", msg_receber.resposta);
        printf("===================================\n");
    }
    else
    {
        printf("Cliente: Erro, esperava uma resposta (tipo 4) e recebi tipo %d\n", msg_receber.tipo);
    }

    printf("Pressione ENTER para fechar...\n");
    getchar();
}
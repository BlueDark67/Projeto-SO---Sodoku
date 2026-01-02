#ifndef SOLVER_H
#define SOLVER_H

#include <pthread.h>

// Estrutura para passar argumentos às threads
typedef struct {
    int id;                 // ID da thread para debug
    int tabuleiro[9][9];    // Cópia local do tabuleiro
    int linha_inicial;      // Linha da célula de arranque
    int coluna_inicial;     // Coluna da célula de arranque
    int numero_arranque;    // Número a testar nessa célula
    int sockfd;             // Socket para validação remota
    int idCliente;          // ID do cliente para protocolo
} ThreadArgs;

/**
 * @brief Resolve um tabuleiro de Sudoku usando Backtracking (Recursivo)
 * 
 * Modifica o tabuleiro in-place.
 * 
 * @param tabuleiro String de 81 caracteres representando o tabuleiro ('0' = vazio)
 * @param sockfd Socket para comunicação com servidor (validação parcial)
 * @param idCliente ID do cliente para mensagens de protocolo
 * @return 1 se encontrou solução, 0 se não tem solução
 */
int resolver_sudoku(char *tabuleiro, int sockfd, int idCliente);

/**
 * @brief Versão paralela do solver
 * 
 * @param tabuleiro_inicial Matriz 9x9 de inteiros
 * @param sockfd Socket para comunicação com servidor
 * @param idCliente ID do cliente para mensagens de protocolo
 * @param numThreads Número máximo de threads a usar (1-9)
 * @return 1 se encontrou solução, 0 caso contrário
 */
int resolver_sudoku_paralelo(int tabuleiro_inicial[9][9], int sockfd, int idCliente, int numThreads);

/**
 * @brief Retorna o número de threads usadas na última resolução
 */
int get_num_threads_last_run();

/**
 * @brief Define o número global de threads a usar
 */
void set_global_num_threads(int num);

#endif

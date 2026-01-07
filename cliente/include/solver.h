#ifndef SOLVER_H
#define SOLVER_H

#include <pthread.h>

// Estrutura para passar argumentos às threads
typedef struct
{
    int id;              // ID da thread para debug
    int tabuleiro[9][9]; // Cópia local do tabuleiro
    int linha_inicial;   // Linha da célula de arranque
    int coluna_inicial;  // Coluna da célula de arranque
    int numero_arranque; // Número a testar nessa célula
    int sockfd;          // Socket para validação remota
    int idCliente;       // ID do cliente para protocolo
} ThreadArgs;

int resolver_sudoku(char *tabuleiro, int sockfd, int idCliente);

int resolver_sudoku_paralelo(int tabuleiro_inicial[9][9], int sockfd, int idCliente, int numThreads);

int get_num_threads_last_run();

void set_global_num_threads(int num);

#endif

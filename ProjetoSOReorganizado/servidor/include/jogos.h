#ifndef JOGOS_H
#define JOGOS_H

#include "config_servidor.h"

// Estrutura para resultado de verificação
typedef struct {
    int correto;           // 1 se totalmente correto, 0 se há erros
    int numerosErrados;    // Quantidade de números errados
    int numerosCertos;     // Quantidade de números certos
} ResultadoVerificacao;

// Carrega jogos do ficheiro
int carregarJogos(const char *ficheiro, Jogo jogos[], int maxJogos);

// Verifica se uma solução está correta
ResultadoVerificacao verificarSolucao(const char *solucao, const char *solucaoCorreta);

// Verifica se um tabuleiro é válido (sem números repetidos em linha/coluna/região)
int validarTabuleiro(const char *tabuleiro);

// Imprime um tabuleiro de forma visual
void imprimirTabuleiro(const char *tabuleiro);

// Converte string de tabuleiro para matriz 9x9
void stringParaMatriz(const char *str, int matriz[9][9]);

// Converte matriz 9x9 para string
void matrizParaString(int matriz[9][9], char *str);

#endif
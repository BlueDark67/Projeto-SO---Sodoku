/*
 * servidor/src/jogos.c
 * 
 * Gestão e Validação de Jogos Sudoku
 * 
 * Este módulo fornece funções para:
 * - Carregar jogos de um ficheiro CSV (formato: id,tabuleiro,solução)
 * - Verificar soluções submetidas pelos clientes
 * - Validar tabuleiros Sudoku (regras de linhas/colunas/regiões)
 * - Contar erros e acertos nas soluções
 * 
 * Formato do ficheiro de jogos:
 * Cada linha: ID,TABULEIRO(81 dígitos),SOLUCAO(81 dígitos)
 * Exemplo: 1,003020600900305001001806400...,483921657967345821...
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "jogos.h"

/**
 * @brief Carrega jogos de um ficheiro de texto
 * 
 * Lê um ficheiro CSV onde cada linha representa um jogo no formato:
 * id,tabuleiro,solucao
 * 
 * @param ficheiro Caminho para o ficheiro de jogos
 * @param jogos Array onde os jogos serão armazenados
 * @param maxJogos Capacidade máxima do array
 * @return Número de jogos carregados com sucesso, ou -1 em erro
 */
int carregarJogos(const char *ficheiro, Jogo jogos[], int maxJogos) {
    printf("DEBUG: Tentando abrir ficheiro: '%s'\n", ficheiro);
    
    FILE *f = fopen(ficheiro, "r");
    if (!f) {
        perror("Erro ao abrir ficheiro de jogos");
        printf("Ficheiro procurado: %s\n", ficheiro);
        printf("Verifique se:\n");
        printf("  1. O ficheiro existe\n");
        printf("  2. O caminho está correto no server.conf\n");
        printf("  3. Tens permissões de leitura\n");
        return -1;
    }
    
    printf("DEBUG: Ficheiro aberto com sucesso!\n");

    int count = 0;
    char linha[300]; // Buffer para cada linha do ficheiro
    int linha_num = 0;
    
    // Processar cada linha do ficheiro
    while (fgets(linha, sizeof(linha), f) && count < maxJogos) {
        linha_num++;
        
        // Remover caracteres de fim de linha
        linha[strcspn(linha, "\r\n")] = 0;
        
        // Ignorar linhas vazias
        if (strlen(linha) == 0) {
            printf("DEBUG: Linha %d vazia, ignorando\n", linha_num);
            continue;
        }
        
        // Debug: mostrar primeiras 3 linhas para verificação
        if (linha_num <= 3) {
            printf("DEBUG: Linha %d (primeiros 50 chars): %.50s...\n", linha_num, linha);
        }
        
        // Parse CSV: dividir por vírgulas
        // Formato esperado: id,tabuleiro,solucao
        char *token = strtok(linha, ",");
        if (!token) {
            printf("DEBUG: Erro no parsing da linha %d - sem ID\n", linha_num);
            continue;
        }
        
        jogos[count].idjogo = atoi(token);
        
        token = strtok(NULL, ",");
        if (!token) {
            printf("DEBUG: Erro no parsing da linha %d - sem tabuleiro\n", linha_num);
            continue;
        }
        
        if (strlen(token) != 81) {
            printf("DEBUG: Erro na linha %d - tabuleiro tem %lu chars (esperado 81)\n", 
                   linha_num, strlen(token));
            continue;
        }
        
        strncpy(jogos[count].tabuleiro, token, 81); //copia os 81 chars de token para tabuleiro
        jogos[count].tabuleiro[81] = '\0';
        

        //verufica solucao
        token = strtok(NULL, ",");
        if (!token) {
            printf("DEBUG: Erro no parsing da linha %d - sem solução\n", linha_num);
            continue;
        }
        
        if (strlen(token) != 81) {
            printf("DEBUG: Erro na linha %d - solução tem %lu chars (esperado 81)\n", 
                   linha_num, strlen(token));
            continue;
        }
        
        strncpy(jogos[count].solucao, token, 81);
        jogos[count].solucao[81] = '\0'; //garante que termina a string
        count++;
        
        if (count <= 3 || count == maxJogos) {
            printf("DEBUG: Jogo %d carregado (ID: %d)\n", count, jogos[count-1].idjogo);
        }
    }
    
    fclose(f);
    printf("\n✓ Carregados %d jogos do ficheiro %s\n", count, ficheiro);
    return count;
}

ResultadoVerificacao verificarSolucao(const char *solucao, const char *solucaoCorreta, const char *puzzleOriginal) {
    ResultadoVerificacao resultado = {1, 0, 0};
    
    if (strlen(solucao) != 81) {
        resultado.correto = 0;
        resultado.numerosErrados = 81;
        return resultado;
    }

    // 1. Verificar se a solução respeita o puzzle original (números fixos)
    int alterouFixo = 0;
    for (int i = 0; i < 81; i++) {
        if (puzzleOriginal[i] != '0' && solucao[i] != puzzleOriginal[i]) {
            alterouFixo = 1;
            break;
        }
    }

    // 2. Verificar se a solução é um Sudoku válido (sem repetições e completo)
    int valido = validarTabuleiro(solucao);
    
    // Verificar se está completo (sem zeros)
    int completo = 1;
    for(int i=0; i<81; i++) {
        if(solucao[i] < '1' || solucao[i] > '9') {
            completo = 0;
            break;
        }
    }

    // Se respeita o puzzle, é válido e está completo -> SUCESSO
    if (!alterouFixo && valido && completo) {
        resultado.correto = 1;
        resultado.numerosCertos = 81;
        resultado.numerosErrados = 0;
        return resultado;
    }

    // Se falhou, comparamos com a solução canónica para dar feedback detalhado
    resultado.correto = 0;
    resultado.numerosCertos = 0;
    resultado.numerosErrados = 0;
    
    for (int i = 0; i < 81; i++) {
        if (solucao[i] == solucaoCorreta[i]) {
            resultado.numerosCertos++;
        } else {
            resultado.numerosErrados++;
        }
    }
    
    return resultado;
}

int validarTabuleiro(const char *tabuleiro) {
    int matriz[9][9];
    stringParaMatriz(tabuleiro, matriz);
    
    // Verificar linhas
    for (int i = 0; i < 9; i++) {
        int nums[10] = {0};
        for (int j = 0; j < 9; j++) {
            int val = matriz[i][j];
            if (val != 0) {
                if (nums[val] > 0) return 0; // Número repetido
                nums[val]++;
            }
        }
    }
    
    // Verificar colunas
    for (int j = 0; j < 9; j++) {
        int nums[10] = {0};
        for (int i = 0; i < 9; i++) {
            int val = matriz[i][j];
            if (val != 0) {
                if (nums[val] > 0) return 0;
                nums[val]++;
            }
        }
    }
    
    // Verificar regiões 3x3
    for (int regiao = 0; regiao < 9; regiao++) {
        int nums[10] = {0};
        int startRow = (regiao / 3) * 3;
        int startCol = (regiao % 3) * 3;
        
        for (int i = startRow; i < startRow + 3; i++) {
            for (int j = startCol; j < startCol + 3; j++) {
                int val = matriz[i][j];
                if (val != 0) {
                    if (nums[val] > 0) return 0;
                    nums[val]++;
                }
            }
        }
    }
    
    return 1; // Tabuleiro válido
}

void imprimirTabuleiro(const char *tabuleiro) {
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            int idx = i * 9 + j;
            printf("%c ", tabuleiro[idx]);
            if (j == 2 || j == 5) printf("| ");
        }
        printf("\n");
        if (i == 2 || i == 5) {
            printf("---------------------\n");
        }
    }
}

void stringParaMatriz(const char *str, int matriz[9][9]) {
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            int idx = i * 9 + j;
            matriz[i][j] = str[idx] - '0';
        }
    }
}

void matrizParaString(int matriz[9][9], char *str) {
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            int idx = i * 9 + j;
            str[idx] = matriz[i][j] + '0';
        }
    }
    str[81] = '\0';
}
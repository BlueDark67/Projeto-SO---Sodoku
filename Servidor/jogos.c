#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "jogos.h"

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
    char linha[300]; //linha so pode ter ate 300 caracteres
    int linha_num = 0;
    
    while (fgets(linha, sizeof(linha), f) && count < maxJogos) {
        linha_num++;
        
        // Remove newline e carriage return
        linha[strcspn(linha, "\r\n")] = 0;
        
        // Ignorar linhas vazias
        if (strlen(linha) == 0) {
            printf("DEBUG: Linha %d vazia, ignorando\n", linha_num);
            continue;
        }
        
        // Debug: mostrar primeiras 3 linhas
        if (linha_num <= 3) {
            printf("DEBUG: Linha %d (primeiros 50 chars): %.50s...\n", linha_num, linha);
        }
        
        // Parse: id,tabuleiro,solucao
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

ResultadoVerificacao verificarSolucao(const char *solucao, const char *solucaoCorreta) {
    ResultadoVerificacao resultado = {1, 0, 0};
    
    if (strlen(solucao) != 81 || strlen(solucaoCorreta) != 81) {
        resultado.correto = 0;
        resultado.numerosErrados = 81;
        return resultado;
    }
    
    for (int i = 0; i < 81; i++) {
        if (solucao[i] == solucaoCorreta[i]) {
            resultado.numerosCertos++;
        } else {
            resultado.numerosErrados++;
            resultado.correto = 0;
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
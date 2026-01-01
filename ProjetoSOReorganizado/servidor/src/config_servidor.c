#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config_servidor.h"

// Remove espaços e caracteres de controle (como \r, \n) do final da string
static void trim(char *str) {
    char *end = str + strlen(str) - 1;
    while (end >= str && (isspace((unsigned char)*end) || *end == '\r')) {
        end--;
    }
    *(end + 1) = '\0';
}

int lerConfigServidor(const char *nomeFicheiro, ConfigServidor *config) {
    FILE *f = fopen(nomeFicheiro, "r");
    if (!f) {
        printf("Erro ao abrir o ficheiro de configuração: %s\n", nomeFicheiro);
        return -1;
    }

    // Inicializar com valores inválidos para forçar configuração explícita
    config->porta = -1;
    config->maxFila = -1;
    config->maxJogos = -1;
    config->delayErro = -1;
    config->maxLinha = -1;
    config->ficheiroJogos[0] = '\0';
    config->ficheiroSolucoes[0] = '\0';
    config->ficheiroLog[0] = '\0';

    char linha[200];
    while (fgets(linha, sizeof(linha), f)) {
        // Ignorar linhas vazias e comentários
        if (linha[0] == '\n' || linha[0] == '#') {
            continue;
        }
        
        char parametro[50], valor[150];
        if (sscanf(linha, "%49[^:]: %149[^\n]", parametro, valor) == 2) {
            trim(valor); // Remove \r e espaços do final
            if (strcmp(parametro, "JOGOS") == 0) {
                strcpy(config->ficheiroJogos, valor);
            } else if (strcmp(parametro, "SOLUCOES") == 0) {
                strcpy(config->ficheiroSolucoes, valor);
            } else if (strcmp(parametro, "LOG") == 0) {
                strcpy(config->ficheiroLog, valor);
            } else if (strcmp(parametro, "PORTA") == 0) {
                config->porta = atoi(valor);
            } else if (strcmp(parametro, "MAX_FILA") == 0) {
                config->maxFila = atoi(valor);
            } else if (strcmp(parametro, "MAX_JOGOS") == 0) {
                config->maxJogos = atoi(valor);
            } else if (strcmp(parametro, "DELAY_ERRO") == 0) {
                config->delayErro = atoi(valor);
            } else if (strcmp(parametro, "MAXLINE") == 0) {
                config->maxLinha = atoi(valor);
            }
        }
    }

    fclose(f);
    return 0;
}

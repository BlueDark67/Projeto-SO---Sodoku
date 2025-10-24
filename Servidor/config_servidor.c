#include <stdio.h>
#include <string.h>
#include "config_servidor.h"

int lerConfigServidor(const char *nomeFicheiro, ConfigServidor *config) {
    FILE *f = fopen(nomeFicheiro, "r");
    if (!f) {
        printf("Erro ao abrir o ficheiro de configuração: %s\n", nomeFicheiro);
        return -1;
    }

    char linha[200];
    while (fgets(linha, sizeof(linha), f)) {
        char parametro[50], valor[150];
        if (sscanf(linha, "%49[^:]: %149[^\n]", parametro, valor) == 2) {
            if (strcmp(parametro, "JOGOS") == 0) {
                strcpy(config->ficheiroJogos, valor);
            } else if (strcmp(parametro, "SOLUCOES") == 0) {
                strcpy(config->ficheiroSolucoes, valor);
            } else if (strcmp(parametro, "LOG") == 0) {
                strcpy(config->ficheiroLog, valor);
            }
        }
    }

    fclose(f);
    return 0;
}

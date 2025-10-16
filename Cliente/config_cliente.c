#include <stdio.h>
#include <string.h>
#include "config_cliente.h"

int lerConfigCliente(const char *nomeFicheiro, ConfigCliente *config)
{
    FILE *f = fopen(nomeFicheiro, "r");
    if (!f)
    {
        printf("Erro ao abrir o ficheiro de configuração: %s\n", nomeFicheiro);
        return -1;
    }

    char linha[200];
    while (fgets(linha, sizeof(linha), f))
    {
        char parametro[50], valor[150];
        if (sscanf(linha, "%49[^:]: %149[^\n]", parametro, valor) == 2)
        {
            if (strcmp(parametro, "ID") == 0)
            {
                strcpy(config->ficheiroID, valor);
            }
            else if (strcmp(parametro, "IP") == 0)
            {
                strcpy(config->ficheiroIP, valor);
            }
            else if (strcmp(parametro, "LOG") == 0)
            {
                strcpy(config->ficheiroLog, valor);
            }
        }
    }

    fclose(f);
    return 0;
}

void printPuzzle(int puzzle[9][9])
{
    int i, j;
    char letras[] = "A B C  D E F  G H I";
    printf("   %s\n", letras);
    printf("  ----------------------\n");
    for (i = 0; i < 9; i++)
    {
        printf("%c |", 'A' + i);
        for (j = 0; j < 9; j++)
        {
            printf("%d ", puzzle[i][j]);
            if ((j + 1) % 3 == 0)
            {
                printf("|");
            }
        }
        printf("\n");
        if ((i + 1) % 3 == 0)
        {
            printf("  ----------------------\n");
        }
    }
}

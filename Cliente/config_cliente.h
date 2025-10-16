#ifndef CONFIG_CLIENTE_H
#define CONFIG_CLIENTE_H

typedef struct
{
    char ficheiroID[100];
    char ficheiroIP[100];
    char ficheiroLog[100];
} ConfigCliente;

int lerConfigCliente(const char *nomeFicheiro, ConfigCliente *config);

void printPuzzle(int puzzle[9][9]);

#endif

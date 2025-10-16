#ifndef CONFIG_SERVIDOR_H
#define CONFIG_SERVIDOR_H

typedef struct
{
    char ficheiroJogos[100];
    char ficheiroSolucoes[100];
    char ficheiroLog[100];
} ConfigServidor;

int lerConfigServidor(const char *nomeFicheiro, ConfigServidor *config);

#endif

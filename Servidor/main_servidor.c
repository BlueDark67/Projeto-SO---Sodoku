#include <stdio.h>
#include "config_servidor.h"

int main()
{
    ConfigServidor config;

    if (lerConfigServidor("server.conf", &config) == 0)
    {
        printf("Ficheiro de jogos: %s\n", config.ficheiroJogos);
        printf("Ficheiro de soluções: %s\n", config.ficheiroSolucoes);
        printf("Ficheiro de logs: %s\n", config.ficheiroLog);
    }
    else
    {
        printf("Falha a ler o ficheiro de configuração!\n");
    }

    return 0;
}

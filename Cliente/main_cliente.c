#include <stdio.h>
#include <stdlib.h>
#include "config_cliente.h"

int main()
{
    ConfigCliente config;

    if (lerConfigCliente("cliente.conf", &config) == 0)
    {
        printf("ID: %s\n", config.ficheiroID);
        printf("IP: %s\n", config.ficheiroIP);
        printf("Logs: %s\n", config.ficheiroLog);
    }
    else
    {
        printf("Falha a ler o ficheiro de configuração!\n");
    }

    return 0;
}
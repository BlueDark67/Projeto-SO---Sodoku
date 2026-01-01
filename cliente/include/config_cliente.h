#ifndef CONFIG_CLIENTE_H
#define CONFIG_CLIENTE_H

typedef struct {
    char ipServidor[50];   // Espaço para um endereço IP (ex: 192.168.1.100)
    int idCliente;         // ID deste cliente
    int porta;             // Porta do servidor
    char ficheiroLog[100]; // Opcional: para o cliente também ter um log
} ConfigCliente;

int lerConfigCliente(const char *nomeFicheiro, ConfigCliente *config);

#endif
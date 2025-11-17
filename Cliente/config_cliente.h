#ifndef CONFIG_CLIENTE_H
#define CONFIG_CLIENTE_H

typedef struct {
    char ipServidor[50];  // Espaço para um endereço IP (ex: 192.168.1.100)
    int idCliente;        // ID deste cliente
    char ficheiroLog[100]; // Opcional: para o cliente também ter um log
} ConfigCliente;

/**
 * @brief Lê o ficheiro de configuração do cliente (ex: cliente.conf).
 * * @param nomeFicheiro O caminho para o ficheiro de configuração.
 * @param config Um ponteiro para a estrutura ConfigCliente onde os dados serão guardados.
 * @return 0 em sucesso, -1 em erro.
 */
int lerConfigCliente(const char *nomeFicheiro, ConfigCliente *config);

#endif
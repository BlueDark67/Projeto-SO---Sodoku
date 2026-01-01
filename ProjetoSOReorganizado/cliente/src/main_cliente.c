/*
 * cliente/main.c
 * Cliente Sudoku - Fase 2
 * Baseado no exemplo unix-stream-client.c, adaptado para AF_INET (Internet)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // <--- MUDANÇA (em vez de sys/un.h)
#include <arpa/inet.h>  // <--- NOVO (para converter IPs)

// Ficheiros do teu projeto
#include "config_cliente.h" // Configuração do cliente
#include "protocolo.h"      // O teu protocolo partilhado (common/include)
#include "util.h"           // Funções utilitárias (readn, writen... common/include)
void str_cli(FILE *fp, int sockfd, int idCliente);

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in serv_addr;
    ConfigCliente config;
    char ficheiroConfig[256];

    printf("===========================================\n");
    printf("   CLIENTE SUDOKU\n");
    printf("===========================================\n\n");

    // --- Verificar argumento de configuração ---
    if (argc >= 2) {
        // User passou ficheiro de configuração
        strncpy(ficheiroConfig, argv[1], sizeof(ficheiroConfig) - 1);
        ficheiroConfig[sizeof(ficheiroConfig) - 1] = '\0';
        
        // Verificar se o ficheiro existe
        if (access(ficheiroConfig, F_OK) != 0) {
            fprintf(stderr, "ERRO: Ficheiro '%s' não encontrado!\n", ficheiroConfig);
            fprintf(stderr, "-> Verifique se o caminho está correto.\n");
            fprintf(stderr, "-> Exemplo: %s config/cliente/cliente.conf\n", argv[0]);
            return 1;
        }
    } else {
        // Usar ficheiro default: cliente.conf
        // Tentar primeiro config/cliente/cliente.conf (executar da raiz)
        if (access("config/cliente/cliente.conf", F_OK) == 0) {
            strncpy(ficheiroConfig, "config/cliente/cliente.conf", sizeof(ficheiroConfig) - 1);
        }
        // Senão tentar ../config/cliente/cliente.conf (executar de build/)
        else if (access("../config/cliente/cliente.conf", F_OK) == 0) {
            strncpy(ficheiroConfig, "../config/cliente/cliente.conf", sizeof(ficheiroConfig) - 1);
        }
        else {
            fprintf(stderr, "ERRO: Ficheiro de configuração default não encontrado\n");
            fprintf(stderr, "-> Procurado: config/cliente/cliente.conf e ../config/cliente/cliente.conf\n");
            fprintf(stderr, "-> Especifique o caminho como argumento.\n");
            fprintf(stderr, "-> Exemplo: %s config/cliente/cliente.conf\n", argv[0]);
            return 1;
        }
        printf("Usando configuração default: %s\n\n", ficheiroConfig);
    }

    // --- Lógica de Configuração do Cliente ---
    printf("1. A ler configuração...\n");
    if (lerConfigCliente(ficheiroConfig, &config) != 0) {
        fprintf(stderr, "Cliente: Falha a ler %s\n", ficheiroConfig);
        return 1;
    }
    
    // Validar configurações obrigatórias
    if (strlen(config.ipServidor) == 0) {
        fprintf(stderr, "ERRO: Configuração 'IP_SERVIDOR' não encontrada em %s\n", ficheiroConfig);
        return 1;
    }
    if (config.idCliente < 0) {
        fprintf(stderr, "ERRO: Configuração 'ID_CLIENTE' não encontrada ou inválida em %s\n", ficheiroConfig);
        return 1;
    }
    if (config.porta <= 0 || config.porta > 65535) {
        fprintf(stderr, "ERRO: PORTA inválida (%d). Deve estar entre 1 e 65535\n", config.porta);
        return 1;
    }
    
    printf("   ✓ IP do Servidor: %s\n", config.ipServidor);
    printf("   ✓ Porta: %d\n", config.porta);
    printf("   ✓ ID do Cliente: %d\n\n", config.idCliente);

    // --- Início da Lógica de Sockets (Fase 2) ---

    /* Cria socket stream (TCP) para Internet */
    printf("2. A criar socket TCP (AF_INET)...\n");
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        err_dump("Cliente: não foi possível abrir o socket stream");
    }

    /* Configura a morada do SERVIDOR (IP + Porta) */
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(config.porta); // Define a porta do servidor

    // Converte o IP de texto (ex: "127.0.0.1") para o formato de rede
    if (inet_pton(AF_INET, config.ipServidor, &serv_addr.sin_addr) <= 0)
    {
        err_dump("Cliente: Morada de IP inválida");
    }

    /* Tenta estabelecer uma ligação ao servidor */
    printf("3. A ligar ao servidor %s:%d...\n", config.ipServidor, config.porta);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        err_dump("Cliente: não foi possível ligar ao servidor");
    }

    printf("   ✓ Ligado com sucesso!\n\n");

    /* Envia os pedidos e recebe as respostas */
    // str_cli é a função do util-stream-cliente.c
    // É AQUI que vais implementar a lógica do protocolo.h
    str_cli(stdin, sockfd, config.idCliente); // Passa o ID do cliente

    /* Fecha o socket e termina */
    printf("\n4. A fechar ligação...\n");
    close(sockfd);
    exit(0);
}
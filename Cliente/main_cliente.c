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
#include "config_cliente.h"  // Precisarás de criar este .h e .c
#include "../protocolo.h"      // O teu protocolo partilhado
#include "util.h"  // Funções utilitárias (readn, writen...)
void str_cli(FILE *fp, int sockfd, int idCliente);            

#define PORTA 8080 // Porta que o servidor está a usar

int main(void) {
    int sockfd;
    struct sockaddr_in serv_addr; // <--- MUDANÇA (sockaddr_in)
    ConfigCliente config; // Precisamos disto para saber o IP e ID

    printf("===========================================\n");
    printf("CLIENTE SUDOKU\n");
    printf("===========================================\n\n");

    // --- Lógica de Configuração do Cliente ---
    // (Isto é novo, mas é pedido no enunciado do projeto)
    printf("1. A ler configuração (cliente.conf)...\n");
    // if (lerConfigCliente("cliente.conf", &config) != 0) {
    //     err_dump("Cliente: Falha a ler cliente.conf");
    // }
    // printf("   ✓ A ligar ao IP: %s\n", config.ipServidor);
    // printf("   ✓ O meu ID é: %d\n\n", config.idCliente);

    // --- (PARA TESTAR SEM O FICHEIRO) ---
    // Vamos definir valores à mão só para testar agora
  if (lerConfigCliente("cliente.conf", &config) != 0) {
    err_dump("Cliente: Falha a ler cliente.conf");
    }
     printf("   ✓ A ligar ao IP: %s\n", config.ipServidor);
     printf("   ✓ O meu ID é: %d\n", config.idCliente);
    // --- (FIM DO TESTE) ---


    // --- Início da Lógica de Sockets (Fase 2) ---

    /* Cria socket stream (TCP) para Internet */
    printf("2. A criar socket TCP (AF_INET)...\n");
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        err_dump("Cliente: não foi possível abrir o socket stream");
    }

    /* Configura a morada do SERVIDOR (IP + Porta) */
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORTA); // Define a porta do servidor

    // Converte o IP de texto (ex: "127.0.0.1") para o formato de rede
    if (inet_pton(AF_INET, config.ipServidor, &serv_addr.sin_addr) <= 0) {
        err_dump("Cliente: Morada de IP inválida");
    }

    /* Tenta estabelecer uma ligação ao servidor */
    printf("3. A ligar ao servidor %s:%d...\n", config.ipServidor, PORTA);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
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
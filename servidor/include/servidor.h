#ifndef SERVIDOR_H
#define SERVIDOR_H

#include <semaphore.h>
#include "config_servidor.h" // <-- Importante: Isto define a struct Jogo

// Estrutura para Memória Partilhada
typedef struct {
    int numClientes;        // Total de clientes conectados (estatística)
    int clientesEmEspera;   // Clientes bloqueados na barreira aguardando par
    sem_t mutex;            // Proteção para acesso à memória partilhada
    sem_t barreira;         // Semáforo para sincronização de pares
} DadosPartilhados;

// Protótipo da função que está em util-stream-server.c
void str_echo(int sockfd, Jogo jogos[], int numJogos, DadosPartilhados *dados, int maxLinha, int timeoutCliente);

#endif
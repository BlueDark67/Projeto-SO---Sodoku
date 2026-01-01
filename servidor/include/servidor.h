#ifndef SERVIDOR_H
#define SERVIDOR_H

#include <semaphore.h>
#include "config_servidor.h" // <-- Importante: Isto define a struct Jogo

// Estrutura para Memória Partilhada
typedef struct {
    int numClientes;
    sem_t mutex;
    sem_t barreira;
} DadosPartilhados;

// Protótipo da função que está em util-stream-server.c
void str_echo(int sockfd, Jogo jogos[], int numJogos, DadosPartilhados *dados, int maxLinha);

#endif
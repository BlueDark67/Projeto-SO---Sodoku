#ifndef SERVIDOR_H
#define SERVIDOR_H

#include <semaphore.h>
#include <time.h>
#include "config_servidor.h" // <-- Importante: Isto define a struct Jogo

// Estrutura para Memória Partilhada (Lobby Dinâmico)
typedef struct {
    int numClientesJogando;     // Total de clientes atualmente jogando (máx: MAX_CLIENTES_JOGO)
    int numClientesLobby;       // Clientes aguardando no lobby
    int numJogadoresAtivos;     // Clientes que estão atualmente a resolver o puzzle
    time_t ultimaEntrada;       // Timestamp da última conexão (para timer de agregação)
    int jogoAtual;              // ID do jogo atual sendo jogado (índice no array de jogos)
    int jogoIniciado;           // Flag: 1 = jogo em curso, 0 = aguardando jogadores
    sem_t mutex;                // Proteção para acesso à memória partilhada
    sem_t lobby_semaforo;       // Semáforo para despertar clientes quando jogo inicia
} DadosPartilhados;

// Protótipo da função que está em util-stream-server.c
void str_echo(int sockfd, Jogo jogos[], int numJogos, DadosPartilhados *dados, int maxLinha, int timeoutCliente);

#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // <--- MUDANÇA (em vez de sys/un.h)
#include <arpa/inet.h>  // <--- NOVO (para converter IPs)
#include <sys/mman.h>   // <--- Necessário para mmap (memória partilhada)

// Ficheiros do teu projeto
#include "config_servidor.h" // Da tua Fase 1
#include "jogos.h"           // Da tua Fase 1
#include "logs.h"            // Da tua Fase 1
#include "protocolo.h"       // O teu protocolo partilhado (common/include)
#include "util.h"            // Funções de rede (common/include)
#include "servidor.h"

#define PORTA 8080 // Porta que o servidor vai usar
#define MAX_FILA 5 // Máximo de clientes em espera

int main(void)
{
    int sockfd, newsockfd, childpid;
    socklen_t clilen;
    struct sockaddr_in cli_addr, serv_addr; // <--- MUDANÇA (sockaddr_in)

    ConfigServidor config;
    Jogo jogos[MAX_JOGOS];
    int numJogos = 0;

    printf("===========================================\n");
    printf("   SERVIDOR SUDOKU\n");
    printf("===========================================\n\n");

    // --- Início da Lógica da Fase 1 ---
    printf("1. A ler configuração...\n");
    if (lerConfigServidor("config/server.conf", &config) != 0)
    {
        err_dump("Servidor: Falha a ler config/server.conf");
    }

    printf("2. A inicializar logs em %s...\n", config.ficheiroLog);
    if (inicializarLog(config.ficheiroLog) != 0)
    {
        err_dump("Servidor: Falha a iniciar logs");
    }
    registarEvento(0, EVT_SERVIDOR_INICIADO, "Servidor a arrancar...");

    printf("3. A carregar jogos de %s...\n", config.ficheiroJogos);
    numJogos = carregarJogos(config.ficheiroJogos, jogos, MAX_JOGOS);
    if (numJogos <= 0)
    {
        registarEvento(0, EVT_ERRO_GERAL, "Nenhum jogo carregado, servidor a fechar.");
        err_dump("Servidor: Falha a carregar jogos");
    }
    registarEvento(0, EVT_JOGOS_CARREGADOS, "Jogos carregados");
    printf("   ✓ %d jogos carregados.\n\n", numJogos);
    // --- Fim da Lógica da Fase 1 ---

    // --- Início da Lógica de Sockets (Fase 2) ---
    printf("3.5 A configurar memória partilhada...\n");

    // Aloca memória que será partilhada entre processos pais e filhos
    DadosPartilhados *dados = mmap(NULL, sizeof(DadosPartilhados),
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (dados == MAP_FAILED)
    {
        err_dump("Erro ao criar memória partilhada");
    }

    dados->numClientes = 0;

    // Inicializa semáforos
    // O '1' no meio significa "partilhado entre processos"
    sem_init(&dados->mutex, 1, 1);    // Mutex começa a 1 (livre)
    sem_init(&dados->barreira, 1, 0); // Barreira começa a 0 (bloqueado)

    /* Cria socket stream (TCP) para Internet */
    printf("4. A criar socket TCP (AF_INET)...\n");
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        err_dump("Servidor: não foi possível abrir o socket stream");
    }

    /* Configura a morada do servidor (IP + Porta) */
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Aceita ligações de qualquer IP
    serv_addr.sin_port = htons(PORTA);             // Define a porta

    /* Associa o socket à morada e porta */
    printf("5. A fazer bind à porta %d...\n", PORTA);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        err_dump("Servidor: não foi possível fazer bind");
    }

    /* Servidor pronto a aceitar clientes */
    printf("6. A escutar na porta %d...\n", PORTA);
    listen(sockfd, MAX_FILA);

    printf("   ✓ Servidor pronto. À espera de clientes.\n\n");

    for (;;)
    {
        clilen = sizeof(cli_addr);

        /* Aceita um novo cliente */
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
            err_dump("Servidor: erro no accept");

        // Regista o IP do cliente que se ligou (útil para o log!)
        char *ip_cliente = inet_ntoa(cli_addr.sin_addr);
        printf("[LOG] Novo cliente conectado do IP: %s\n", ip_cliente);
        registarEvento(0, EVT_CLIENTE_CONECTADO, ip_cliente);

        /* Lança processo filho para lidar com o cliente */
        if ((childpid = fork()) < 0)
            err_dump("Servidor: erro no fork");

        else if (childpid == 0)
        {
            /* PROCESSO FILHO */
            close(sockfd); // Filho não precisa do socket "pai"

            // str_echo é a função do util-stream-server.c
            // É AQUI que vais implementar a lógica do protocolo.h
            str_echo(newsockfd, jogos, numJogos, dados);

            printf("[LOG] Cliente %s desconectado.\n", ip_cliente);
            registarEvento(0, EVT_CLIENTE_DESCONECTADO, ip_cliente);
            exit(0);
        }

        /* PROCESSO PAI */
        close(newsockfd); // Pai não precisa do socket do cliente
    }
    munmap(dados, sizeof(DadosPartilhados));
}
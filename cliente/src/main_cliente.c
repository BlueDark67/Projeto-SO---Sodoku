// cliente/src/main_cliente.c - Programa principal do cliente Sudoku

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "config_cliente.h"
#include "protocolo.h"
#include "util.h"
#include "logs_cliente.h"
#include "solver.h"

// Declaração da função principal de comunicação
void str_cli(FILE *fp, int sockfd, int idCliente);

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in serv_addr;
    ConfigCliente config;
    char ficheiroConfig[256];

    printf("\033[1;36m");
    printf("╔════════════════════════════════════════╗\n");
    printf("║      CLIENTE SUDOKU - MULTIPLAYER     ║\n");
    printf("╚════════════════════════════════════════╝\033[0m\n\n");

    if (argc >= 2)
    {
        strncpy(ficheiroConfig, argv[1], sizeof(ficheiroConfig) - 1);
        ficheiroConfig[sizeof(ficheiroConfig) - 1] = '\0';

        if (access(ficheiroConfig, F_OK) != 0)
        {
            erro("Ficheiro '%s' não encontrado!", ficheiroConfig);
            aviso("Verifique se o caminho está correto.");
            aviso("Exemplo: %s config/cliente/cliente.conf", argv[0]);
            return 1;
        }
    }
    else
    {
        if (ajustarCaminho("config/cliente/cliente.conf", ficheiroConfig, sizeof(ficheiroConfig)) == 0)
        {
            // Silencioso
        }
        else
        {
            erro("Ficheiro de configuração default não encontrado");
            aviso("Procurado: config/cliente/cliente.conf e ../config/cliente/cliente.conf");
            aviso("Especifique o caminho como argumento.");
            aviso("Exemplo: %s config/cliente/cliente.conf", argv[0]);
            return 1;
        }
    }

    if (lerConfigCliente(ficheiroConfig, &config) != 0)
    {
        erro("Cliente: Falha a ler %s", ficheiroConfig);
        return 1;
    }

    // Validar campos obrigatórios da configuração
    // Sem estas configurações, o cliente não pode funcionar
    if (strlen(config.ipServidor) == 0)
    {
        erro("Configuração 'IP_SERVIDOR' não encontrada em %s", ficheiroConfig);
        return 1;
    }
    if (config.idCliente < 0)
    {
        erro("Configuração 'ID_CLIENTE' não encontrada ou inválida em %s", ficheiroConfig);
        return 1;
    }
    if (config.porta <= 0 || config.porta > 65535)
    {
        erro("PORTA inválida (%d). Deve estar entre 1 e 65535", config.porta);
        return 1;
    }
    if (config.timeoutServidor < 0)
    {
        erro("TIMEOUT_SERVIDOR não configurado ou inválido em %s", ficheiroConfig);
        aviso("Adicione: TIMEOUT_SERVIDOR: 10 (por exemplo)");
        return 1;
    }

    printf("   IP do Servidor: %s\n", config.ipServidor);
    printf("   Porta: %d\n", config.porta);
    printf("   Threads Paralelas: %d\n", config.numThreads);

    // Usar PID como ID único do cliente
    int idCliente = getpid();
    printf("   ID do Cliente (PID): %d\n\n", idCliente);

    // Configurar número de threads no solver
    set_global_num_threads(config.numThreads);

    // Inicializar logs do cliente com ID baseado em PID
    // Determinar se estamos em build/ ou raiz usando o ficheiro de config como referência
    char log_path[256];
    int precisaAjuste = 0;

    // Se o ficheiro de config usado tem ../ no caminho, então estamos em build/
    if (strstr(ficheiroConfig, "../") != NULL)
    {
        precisaAjuste = 1;
    }

    if (precisaAjuste)
    {
        snprintf(log_path, sizeof(log_path), "../logs/clientes/cliente_%d.log", idCliente);
    }
    else
    {
        snprintf(log_path, sizeof(log_path), "logs/clientes/cliente_%d.log", idCliente);
    }

    if (inicializarLogCliente(log_path, idCliente) == 0)
    {
        char msg_init[256];
        snprintf(msg_init, sizeof(msg_init),
                 "Cliente #%d iniciado - Config: %s",
                 idCliente, ficheiroConfig);
        registarEventoCliente(EVTC_CLIENTE_INICIADO, msg_init);
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        err_dump("Cliente: não foi possível abrir o socket stream");
    }

    /* Aplicar timeout de socket */
    struct timeval timeout;
    timeout.tv_sec = config.timeoutServidor;
    timeout.tv_usec = 0;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("Aviso: Falha ao configurar SO_RCVTIMEO");
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("Aviso: Falha ao configurar SO_SNDTIMEO");
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(config.porta); // Define a porta do servidor

    // Converte o IP de texto (ex: "127.0.0.1") para o formato de rede
    if (inet_pton(AF_INET, config.ipServidor, &serv_addr.sin_addr) <= 0)
    {
        err_dump("Cliente: Morada de IP inválida");
    }

    printf("\033[1mServidor:\033[0m %s:%d | \033[1mID:\033[0m %d\n", config.ipServidor, config.porta, idCliente);
    printf("\033[33mA conectar...\033[0m ");
    fflush(stdout);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        char msg_erro[256];
        snprintf(msg_erro, sizeof(msg_erro),
                 "Falha ao conectar a %s:%d", config.ipServidor, config.porta);
        registarEventoCliente(EVTC_ERRO, msg_erro);
        err_dump("Cliente: não foi possível ligar ao servidor");
    }

    printf("\033[32mConectado!\033[0m\n\n");

    char msg_conexao[256];
    snprintf(msg_conexao, sizeof(msg_conexao),
             "Conexão estabelecida com servidor %s:%d",
             config.ipServidor, config.porta);
    registarEventoCliente(EVTC_CONEXAO_ESTABELECIDA, msg_conexao);

    /* Envia os pedidos e recebe as respostas */
    // str_cli é a função do util-stream-cliente.c
    // É AQUI que vais implementar a lógica do protocolo.h
    str_cli(stdin, sockfd, idCliente);

    fecharLogCliente();
    close(sockfd);
    exit(0);
}
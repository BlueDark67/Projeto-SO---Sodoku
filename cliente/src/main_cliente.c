/*
 * cliente/src/main_cliente.c
 * 
 * Programa principal do Cliente Sudoku
 * 
 * Este cliente:
 * - Carrega configurações de ficheiro .conf
 * - Conecta-se ao servidor via TCP/IP
 * - Inicializa sistema de logs
 * - Pede jogos, simula resolução e envia soluções
 * - Recebe e apresenta resultados
 * - Trata path resolution para funcionar em diferentes diretórios
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>       // Para struct timeval (timeout)
#include <netinet/in.h> // Estruturas para sockets TCP/IP
#include <arpa/inet.h>  // Conversão de endereços IP

// Headers do projeto
#include "config_cliente.h" // Gestão de configuração
#include "protocolo.h"      // Protocolo de comunicação
#include "util.h"           // Funções auxiliares de rede
#include "logs_cliente.h"   // Sistema de logging do cliente
#include "solver.h"         // Funções do solver (para set_global_num_threads)

// Declaração da função principal de comunicação
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
            erro("Ficheiro '%s' não encontrado!", ficheiroConfig);
            aviso("Verifique se o caminho está correto.");
            aviso("Exemplo: %s config/cliente/cliente.conf", argv[0]);
            return 1;
        }
    } else {
        // Usar ficheiro default: cliente.conf
        if (ajustarCaminho("config/cliente/cliente.conf", ficheiroConfig, sizeof(ficheiroConfig)) == 0) {
             printf("Usando configuração default: %s\n\n", ficheiroConfig);
        } else {
            erro("Ficheiro de configuração default não encontrado");
            aviso("Procurado: config/cliente/cliente.conf e ../config/cliente/cliente.conf");
            aviso("Especifique o caminho como argumento.");
            aviso("Exemplo: %s config/cliente/cliente.conf", argv[0]);
            return 1;
        }
    }

    // --- Lógica de Configuração do Cliente ---
    printf("1. A ler configuração...\n");
    if (lerConfigCliente(ficheiroConfig, &config) != 0) {
        erro("Cliente: Falha a ler %s", ficheiroConfig);
        return 1;
    }
    
    // Validar campos obrigatórios da configuração
    // Sem estas configurações, o cliente não pode funcionar
    if (strlen(config.ipServidor) == 0) {
        erro("Configuração 'IP_SERVIDOR' não encontrada em %s", ficheiroConfig);
        return 1;
    }
    if (config.idCliente < 0) {
        erro("Configuração 'ID_CLIENTE' não encontrada ou inválida em %s", ficheiroConfig);
        return 1;
    }
    if (config.porta <= 0 || config.porta > 65535) {
        erro("PORTA inválida (%d). Deve estar entre 1 e 65535", config.porta);
        return 1;
    }
    if (config.timeoutServidor < 0) {
        erro("TIMEOUT_SERVIDOR não configurado ou inválido em %s", ficheiroConfig);
        aviso("Adicione: TIMEOUT_SERVIDOR: 10 (por exemplo)");
        return 1;
    }
    
    printf("   ✓ IP do Servidor: %s\n", config.ipServidor);
    printf("   ✓ Porta: %d\n", config.porta);
    printf("   ✓ Threads Paralelas: %d\n", config.numThreads);
    
    // Usar PID como ID único do cliente
    int idCliente = getpid();
    printf("   ✓ ID do Cliente (PID): %d\n\n", idCliente);
    
    // Configurar número de threads no solver
    set_global_num_threads(config.numThreads);
    
    // Inicializar logs do cliente com ID baseado em PID
    // Determinar se estamos em build/ ou raiz usando o ficheiro de config como referência
    char log_path[256];
    int precisaAjuste = 0;
    
    // Se o ficheiro de config usado tem ../ no caminho, então estamos em build/
    if (strstr(ficheiroConfig, "../") != NULL) {
        precisaAjuste = 1;
    }
    
    if (precisaAjuste) {
        snprintf(log_path, sizeof(log_path), "../logs/clientes/cliente_%d.log", idCliente);
    } else {
        snprintf(log_path, sizeof(log_path), "logs/clientes/cliente_%d.log", idCliente);
    }
    
    if (inicializarLogCliente(log_path, idCliente) == 0) {
        printf("   ✓ Logs ativados: %s\n\n", log_path);
        char msg_init[256];
        snprintf(msg_init, sizeof(msg_init), 
                 "Cliente #%d iniciado - Config: %s", 
                 idCliente, ficheiroConfig);
        registarEventoCliente(EVTC_CLIENTE_INICIADO, msg_init);
    }

    // --- Início da Lógica de Sockets (Fase 2) ---

    /* Cria socket stream (TCP) para Internet */
    printf("2. A criar socket TCP (AF_INET)...\n");
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        err_dump("Cliente: não foi possível abrir o socket stream");
    }

    /* Aplicar timeout de socket */
    struct timeval timeout;
    timeout.tv_sec = config.timeoutServidor;
    timeout.tv_usec = 0;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Aviso: Falha ao configurar SO_RCVTIMEO");
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Aviso: Falha ao configurar SO_SNDTIMEO");
    }
    printf("   ✓ Timeout configurado: %d segundos\n", config.timeoutServidor);

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
        char msg_erro[256];
        snprintf(msg_erro, sizeof(msg_erro), 
                 "Falha ao conectar a %s:%d", config.ipServidor, config.porta);
        registarEventoCliente(EVTC_ERRO, msg_erro);
        err_dump("Cliente: não foi possível ligar ao servidor");
    }

    printf("   ✓ Ligado com sucesso!\n\n");
    
    char msg_conexao[256];
    snprintf(msg_conexao, sizeof(msg_conexao), 
             "Conexão estabelecida com servidor %s:%d", 
             config.ipServidor, config.porta);
    registarEventoCliente(EVTC_CONEXAO_ESTABELECIDA, msg_conexao);

    /* Envia os pedidos e recebe as respostas */
    // str_cli é a função do util-stream-cliente.c
    // É AQUI que vais implementar a lógica do protocolo.h
    str_cli(stdin, sockfd, idCliente); // Passa o PID do cliente como ID

    /* Fecha o socket e termina */
    printf("\n4. A fechar ligação...\n");
    fecharLogCliente();
    close(sockfd);
    exit(0);
}
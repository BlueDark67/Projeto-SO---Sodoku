#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // <--- MUDANÇA (em vez de sys/un.h)
#include <arpa/inet.h>  // <--- NOVO (para converter IPs)
#include <sys/mman.h>   // <--- Necessário para mmap (memória partilhada)
#include <dirent.h>     // Para listar diretórios

// Ficheiros do teu projeto
#include "config_servidor.h" // Da tua Fase 1
#include "jogos.h"           // Da tua Fase 1
#include "logs.h"            // Da tua Fase 1
#include "protocolo.h"       // O teu protocolo partilhado (common/include)
#include "util.h"            // Funções de rede (common/include)
#include "servidor.h"

#define PORTA 8080 // Porta que o servidor vai usar
#define MAX_FILA 5 // Máximo de clientes em espera
#define CONFIG_DIR "config/servidor"
#define MAX_CONFIGS 50

// Procura ficheiros .conf no diretório especificado
int procurar_configs(const char *dir, char configs[][256], int max_configs) {
    DIR *d;
    struct dirent *entry;
    int count = 0;
    
    d = opendir(dir);
    if (!d) {
        return 0;
    }
    
    while ((entry = readdir(d)) != NULL && count < max_configs) {
        if (strstr(entry->d_name, ".conf") != NULL) {
            int needed = snprintf(configs[count], 256, "%s/%s", dir, entry->d_name);
            if (needed >= 256) {
                // Caminho muito longo, ignorar
                continue;
            }
            count++;
        }
    }
    
    closedir(d);
    return count;
}

// Procura configs em múltiplos diretórios possíveis
int procurar_configs_multi(char configs[][256], int max_configs) {
    const char *dirs[] = {"config/servidor", "../config/servidor", NULL};
    int total = 0;
    
    for (int i = 0; dirs[i] != NULL && total < max_configs; i++) {
        total += procurar_configs(dirs[i], &configs[total], max_configs - total);
    }
    
    return total;
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, childpid;
    socklen_t clilen;
    struct sockaddr_in cli_addr, serv_addr; // <--- MUDANÇA (sockaddr_in)

    ConfigServidor config;
    Jogo jogos[MAX_JOGOS];
    int numJogos = 0;
    char ficheiroConfig[256];

    printf("===========================================\n");
    printf("   SERVIDOR SUDOKU\n");
    printf("===========================================\n\n");

    // --- Verificar argumento de configuração ---
    if (argc >= 2) {
        // User passou ficheiro de configuração
        strncpy(ficheiroConfig, argv[1], sizeof(ficheiroConfig) - 1);
    } else {
        // Não passou argumento - mostrar 3 mensagens de erro
        printf("ERRO: Nenhum ficheiro de configuração especificado!\n");
        sleep(2);
        printf("ERRO: O servidor precisa de um ficheiro de configuração para iniciar.\n");
        sleep(2);
        printf("ERRO: Use: %s <ficheiro_configuracao>\n\n", argv[0]);
        sleep(3);
        
        // Procurar ficheiros .conf em múltiplos locais
        char configs[MAX_CONFIGS][256];
        int num_configs = procurar_configs_multi(configs, MAX_CONFIGS);
        
        if (num_configs == 0) {
            printf("ERRO FATAL: Não foi encontrado nenhum ficheiro de configuração\n");
            printf("-> Procurado em: config/servidor/ e ../config/servidor/\n");
            printf("-> O servidor precisa de um ficheiro .conf para funcionar.\n");
            printf("-> Crie um ficheiro de configuração ou especifique o caminho como argumento.\n");
            printf("-> Exemplo: %s config/servidor/server.conf\n", argv[0]);
            return 1;
        }
        
        // Mostrar lista de configurações encontradas
        printf("Ficheiros de configuração encontrados para o servidor:\n");
        for (int i = 0; i < num_configs; i++) {
            printf("   %d) %s\n", i + 1, configs[i]);
        }
        
        // Pedir ao user para escolher
        int escolha = -1;
        while (escolha < 1 || escolha > num_configs) {
            printf("\n-> Escolha um ficheiro (1-%d): ", num_configs);
            if (scanf("%d", &escolha) != 1) {
                while (getchar() != '\n'); // Limpar buffer
                escolha = -1;
            }
            
            if (escolha < 1 || escolha > num_configs) {
                printf("Escolha inválida! Tente novamente.\n");
            }
        }
        
        strncpy(ficheiroConfig, configs[escolha - 1], sizeof(ficheiroConfig) - 1);
        printf("\nUsando: %s\n\n", ficheiroConfig);
    }

    // --- Início da Lógica da Fase 1 ---
    printf("1. A ler configuração...\n");
    if (lerConfigServidor(ficheiroConfig, &config) != 0)
    {
        fprintf(stderr, "Servidor: Falha a ler %s\n", ficheiroConfig);
        return 1;
    }

    // Ajustar caminhos se necessário (tentar com e sem ../)
    FILE *test = fopen(config.ficheiroLog, "r");
    if (!test) {
        // Tentar com ../
        char temp[256];
        snprintf(temp, sizeof(temp), "../%s", config.ficheiroLog);
        test = fopen(temp, "r");
        if (test) {
            fclose(test);
            // Adicionar ../ aos caminhos usando buffer temporário
            char tempJogos[256], tempSolucoes[256];
            snprintf(temp, sizeof(temp), "../%s", config.ficheiroLog);
            snprintf(tempJogos, sizeof(tempJogos), "../%s", config.ficheiroJogos);
            snprintf(tempSolucoes, sizeof(tempSolucoes), "../%s", config.ficheiroSolucoes);
            
            strncpy(config.ficheiroLog, temp, sizeof(config.ficheiroLog) - 1);
            strncpy(config.ficheiroJogos, tempJogos, sizeof(config.ficheiroJogos) - 1);
            strncpy(config.ficheiroSolucoes, tempSolucoes, sizeof(config.ficheiroSolucoes) - 1);
        }
    } else {
        fclose(test);
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
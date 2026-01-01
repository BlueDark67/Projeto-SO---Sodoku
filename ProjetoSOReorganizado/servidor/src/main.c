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
    Jogo *jogos;
    int numJogos = 0;
    char ficheiroConfig[256];

    printf("===========================================\n");
    printf("   SERVIDOR SUDOKU\n");
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
            fprintf(stderr, "-> Exemplo: %s config/servidor/server.conf\n", argv[0]);
            return 1;
        }
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

    // Validar configurações obrigatórias
    if (strlen(config.ficheiroJogos) == 0) {
        fprintf(stderr, "ERRO: Configuração 'JOGOS' não encontrada em %s\n", ficheiroConfig);
        return 1;
    }
    if (strlen(config.ficheiroSolucoes) == 0) {
        fprintf(stderr, "ERRO: Configuração 'SOLUCOES' não encontrada em %s\n", ficheiroConfig);
        return 1;
    }
    if (strlen(config.ficheiroLog) == 0) {
        fprintf(stderr, "ERRO: Configuração 'LOG' não encontrada em %s\n", ficheiroConfig);
        return 1;
    }
    
    // Validar configurações numéricas
    if (config.porta <= 0 || config.porta > 65535) {
        fprintf(stderr, "ERRO: PORTA inválida (%d). Deve estar entre 1 e 65535\n", config.porta);
        return 1;
    }
    if (config.maxFila <= 0) {
        fprintf(stderr, "ERRO: MAX_FILA inválida (%d). Deve ser maior que 0\n", config.maxFila);
        return 1;
    }
    if (config.maxJogos <= 0) {
        fprintf(stderr, "ERRO: MAX_JOGOS inválido (%d). Deve ser maior que 0\n", config.maxJogos);
        return 1;
    }
    if (config.delayErro < 0) {
        fprintf(stderr, "ERRO: DELAY_ERRO inválido (%d). Deve ser >= 0\n", config.delayErro);
        return 1;
    }
    if (config.maxLinha < 256) {
        fprintf(stderr, "ERRO: MAXLINE inválido (%d). Deve ser >= 256\n", config.maxLinha);
        return 1;
    }

    // Ajustar caminhos se necessário (tentar com e sem ../)
    // Usa o ficheiro de JOGOS como referência (sempre existe, ao contrário do log)
    FILE *test = fopen(config.ficheiroJogos, "r");
    if (!test) {
        // Tentar com ../
        char temp[256];
        snprintf(temp, sizeof(temp), "../%s", config.ficheiroJogos);
        test = fopen(temp, "r");
        if (test) {
            fclose(test);
            // Adicionar ../ aos caminhos usando buffer temporário
            char tempJogos[256], tempSolucoes[256], tempLog[256];
            snprintf(tempJogos, sizeof(tempJogos), "../%s", config.ficheiroJogos);
            snprintf(tempSolucoes, sizeof(tempSolucoes), "../%s", config.ficheiroSolucoes);
            snprintf(tempLog, sizeof(tempLog), "../%s", config.ficheiroLog);
            
            strncpy(config.ficheiroJogos, tempJogos, sizeof(config.ficheiroJogos) - 1);
            strncpy(config.ficheiroSolucoes, tempSolucoes, sizeof(config.ficheiroSolucoes) - 1);
            strncpy(config.ficheiroLog, tempLog, sizeof(config.ficheiroLog) - 1);
        }
    } else {
        fclose(test);
    }

    printf("2. A inicializar logs em %s...\n", config.ficheiroLog);
    if (inicializarLog(config.ficheiroLog) != 0)
    {
        err_dump("Servidor: Falha a iniciar logs");
    }
    
    char log_init[512];
    snprintf(log_init, sizeof(log_init), 
             "Servidor iniciado - Porta: %d, MaxFila: %d, MaxJogos: %d", 
             config.porta, config.maxFila, config.maxJogos);
    registarEvento(0, EVT_SERVIDOR_INICIADO, log_init);

    printf("3. A alocar memória para %d jogos...\n", config.maxJogos);
    jogos = malloc(sizeof(Jogo) * config.maxJogos);
    if (!jogos) {
        registarEvento(0, EVT_ERRO_GERAL, "Falha ao alocar memória para jogos");
        err_dump("Servidor: Falha ao alocar memória para jogos");
    }

    printf("4. A carregar jogos de %s...\n", config.ficheiroJogos);
    numJogos = carregarJogos(config.ficheiroJogos, jogos, config.maxJogos);
    if (numJogos <= 0)
    {
        registarEvento(0, EVT_ERRO_GERAL, "Nenhum jogo carregado, servidor a fechar.");
        free(jogos);
        err_dump("Servidor: Falha a carregar jogos");
    }
    
    snprintf(log_init, sizeof(log_init), 
             "%d jogos carregados de %s (capacidade: %d jogos)", 
             numJogos, config.ficheiroJogos, config.maxJogos);
    registarEvento(0, EVT_JOGOS_CARREGADOS, log_init);
    printf("   ✓ %d jogos carregados.\n\n", numJogos);
    // --- Fim da Lógica da Fase 1 ---

    // --- Início da Lógica de Sockets (Fase 2) ---
    printf("5. A configurar memória partilhada...\n");

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
    printf("6. A criar socket TCP (AF_INET)...\n");
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        err_dump("Servidor: não foi possível abrir o socket stream");
    }

    /* Configura a morada do servidor (IP + Porta) */
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Aceita ligações de qualquer IP
    serv_addr.sin_port = htons(config.porta);      // Define a porta

    /* Associa o socket à morada e porta */
    printf("7. A fazer bind à porta %d...\n", config.porta);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        err_dump("Servidor: não foi possível fazer bind");
    }

    /* Servidor pronto a aceitar clientes */
    printf("8. A escutar na porta %d (fila máxima: %d)...\n", config.porta, config.maxFila);
    listen(sockfd, config.maxFila);

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
        
        snprintf(log_init, sizeof(log_init), 
                 "Novo cliente conectado de %s (porta %d)", 
                 ip_cliente, ntohs(cli_addr.sin_port));
        registarEvento(0, EVT_CLIENTE_CONECTADO, log_init);

        /* Lança processo filho para lidar com o cliente */
        if ((childpid = fork()) < 0)
            err_dump("Servidor: erro no fork");

        else if (childpid == 0)
        {
            /* PROCESSO FILHO */
            close(sockfd); // Filho não precisa do socket "pai"

            // str_echo é a função do util-stream-server.c
            // É AQUI que vais implementar a lógica do protocolo.h
            str_echo(newsockfd, jogos, numJogos, dados, config.maxLinha);

            printf("[LOG] Cliente %s desconectado.\n", ip_cliente);
            snprintf(log_init, sizeof(log_init), 
                     "Cliente desconectado: %s", ip_cliente);
            registarEvento(0, EVT_CLIENTE_DESCONECTADO, log_init);
            exit(0);
        }

        /* PROCESSO PAI */
        close(newsockfd); // Pai não precisa do socket do cliente
    }
    
    // Cleanup (nunca deve chegar aqui mas está correto)
    free(jogos);
    munmap(dados, sizeof(DadosPartilhados));
}
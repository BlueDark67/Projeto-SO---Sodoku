// servidor/src/main.c - Programa principal do servidor Sudoku

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <dirent.h>
#include <signal.h>
#include <pthread.h>

#include "config_servidor.h"
#include "jogos.h"
#include "logs.h"
#include "protocolo.h"
#include "util.h"
#include "servidor.h"

#define CONFIG_DIR "config/servidor"
#define MAX_CONFIGS 50

static Jogo *jogos_global = NULL;
static int sockfd_global = -1;
static DadosPartilhados *dados_global = NULL;
static ConfigServidor config_global;
static int sou_processo_pai = 1;
static int numJogos_global = 0;
static pthread_t timer_thread;

// Thread que dispara o jogo quando o tempo de agregação expira
void *lobby_timer_thread(void *arg)
{
    (void)arg;

    while (1)
    {
        sleep(1);

        sem_wait(&dados_global->mutex);

        if (dados_global->numClientesLobby >= 2 && dados_global->jogoIniciado == 0)
        {
            time_t agora = time(NULL);
            double tempo_decorrido = difftime(agora, dados_global->ultimaEntrada);
            int restantes = config_global.tempoAgregacao - (int)tempo_decorrido;

            if (restantes > 0)
            {
                printf("\r\033[33mLobby: %d jogadores | Timer: %ds\033[0m        ",
                       dados_global->numClientesLobby, restantes);
                fflush(stdout);
            }

            if (tempo_decorrido >= config_global.tempoAgregacao)
            {
                printf("\n\033[32mJogo #%d iniciado - %d jogadores\033[0m\n",
                       dados_global->jogoAtual, dados_global->numClientesLobby);

                dados_global->jogoAtual = rand() % numJogos_global;
                dados_global->jogoIniciado = 1;
                dados_global->jogoTerminado = 0;
                dados_global->idVencedor = -1;
                dados_global->tempoVitoria = 0;

                char log_msg[256];
                snprintf(log_msg, sizeof(log_msg),
                         "Jogo #%d iniciado por timeout - %d jogadores",
                         dados_global->jogoAtual, dados_global->numClientesLobby);
                registarEvento(0, EVT_SERVIDOR_INICIADO, log_msg);

                for (int i = 0; i < dados_global->numClientesLobby; i++)
                {
                    sem_post(&dados_global->lobby_semaforo);
                }
            }
        }

        sem_post(&dados_global->mutex);
    }

    return NULL;
}

// Liberta todos os recursos antes de terminar o servidor
void cleanup_servidor(void)
{
    printf("\n\033[33mA encerrar servidor...\033[0m\n");

    if (jogos_global != NULL)
    {
        free(jogos_global);
        jogos_global = NULL;
    }

    if (sockfd_global >= 0)
    {
        close(sockfd_global);
        sockfd_global = -1;
    }

    if (dados_global != NULL)
    {
        sem_destroy(&dados_global->mutex);
        sem_destroy(&dados_global->lobby_semaforo);
        munmap(dados_global, sizeof(DadosPartilhados));
        dados_global = NULL;
    }

    fecharLog();

    if (sou_processo_pai && config_global.modo == MODO_DEBUG && config_global.limparLogsEncerramento)
    {
        system("rm -f logs/servidor/*.log");
        system("rm -f logs/clientes/*.log");
    }

    printf("\033[32mServidor terminado.\033[0m\n");
}

// Handler para SIGINT (Ctrl+C)
void sigint_handler(int sig)
{
    (void)sig;
    printf("\n[SINAL] Recebido SIGINT (Ctrl+C)\n");
    cleanup_servidor();
    exit(0);
}

// Procura ficheiros .conf num diretório
int procurar_configs(const char *dir, char configs[][256], int max_configs)
{
    DIR *d;
    struct dirent *entry;
    int count = 0;

    d = opendir(dir);
    if (!d)
    {
        return 0;
    }

    while ((entry = readdir(d)) != NULL && count < max_configs)
    {
        if (strstr(entry->d_name, ".conf") != NULL)
        {
            int needed = snprintf(configs[count], 256, "%s/%s", dir, entry->d_name);
            if (needed >= 256)
            {
                continue;
            }
            count++;
        }
    }

    closedir(d);
    return count;
}

// Procura ficheiros de configuração em vários diretórios possíveis
int procurar_configs_multi(char configs[][256], int max_configs)
{
    char dirPath[256];
    int total = 0;

    if (ajustarCaminho("config/servidor", dirPath, sizeof(dirPath)) == 0)
    {
        total = procurar_configs(dirPath, configs, max_configs);
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

    /* ========================================
     * FASE 1: INICIALIZAÇÃO E CONFIGURAÇÃO
     * ======================================== */

    // --- Verificar argumento de configuração ---
    // Aceita ficheiro de configuração como argumento ou usa default
    if (argc >= 2)
    {
        // User passou ficheiro de configuração
        strncpy(ficheiroConfig, argv[1], sizeof(ficheiroConfig) - 1);
        ficheiroConfig[sizeof(ficheiroConfig) - 1] = '\0';

        // Verificar se o ficheiro existe
        if (access(ficheiroConfig, F_OK) != 0)
        {
            erro("Ficheiro '%s' não encontrado!", ficheiroConfig);
            aviso("Verifique se o caminho está correto.");
            aviso("Exemplo: %s config/servidor/server.conf", argv[0]);
            return 1;
        }
    }
    else
    {
        erro("Nenhum ficheiro de configuração especificado!");
        aviso("Use: %s <ficheiro_configuracao>\n", argv[0]);

        // Procurar ficheiros .conf em múltiplos locais
        char configs[MAX_CONFIGS][256];
        int num_configs = procurar_configs_multi(configs, MAX_CONFIGS);

        if (num_configs == 0)
        {
            erro("FATAL: Não foi encontrado nenhum ficheiro de configuração");
            aviso("Procurado em: config/servidor/ e ../config/servidor/");
            aviso("O servidor precisa de um ficheiro .conf para funcionar.");
            aviso("Crie um ficheiro de configuração ou especifique o caminho como argumento.");
            aviso("Exemplo: %s config/servidor/server.conf", argv[0]);
            return 1;
        }

        // Mostrar lista de configurações encontradas
        printf("Ficheiros de configuração encontrados para o servidor:\n");
        for (int i = 0; i < num_configs; i++)
        {
            printf("   %d) %s\n", i + 1, configs[i]);
        }

        // Pedir ao user para escolher
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "\n-> Escolha um ficheiro (1-%d): ", num_configs);
        int escolha = lerInteiro(prompt, 1, num_configs);

        strncpy(ficheiroConfig, configs[escolha - 1], sizeof(ficheiroConfig) - 1);
        printf("\nUsando: %s\n\n", ficheiroConfig);
    }

    if (lerConfigServidor(ficheiroConfig, &config) != 0)
    {
        fprintf(stderr, "Servidor: Falha a ler %s\n", ficheiroConfig);
        return 1;
    }

    // Validar configurações obrigatórias
    if (strlen(config.ficheiroJogos) == 0)
    {
        fprintf(stderr, "ERRO: Configuração 'JOGOS' não encontrada em %s\n", ficheiroConfig);
        return 1;
    }
    if (strlen(config.ficheiroSolucoes) == 0)
    {
        fprintf(stderr, "ERRO: Configuração 'SOLUCOES' não encontrada em %s\n", ficheiroConfig);
        return 1;
    }
    if (strlen(config.ficheiroLog) == 0)
    {
        fprintf(stderr, "ERRO: Configuração 'LOG' não encontrada em %s\n", ficheiroConfig);
        return 1;
    }

    // Validar configurações numéricas
    if (config.porta <= 0 || config.porta > 65535)
    {
        fprintf(stderr, "ERRO: PORTA inválida (%d). Deve estar entre 1 e 65535\n", config.porta);
        return 1;
    }
    if (config.maxFila <= 0)
    {
        fprintf(stderr, "ERRO: MAX_FILA inválida (%d). Deve ser maior que 0\n", config.maxFila);
        return 1;
    }
    if (config.maxJogos <= 0)
    {
        fprintf(stderr, "ERRO: MAX_JOGOS inválido (%d). Deve ser maior que 0\n", config.maxJogos);
        return 1;
    }
    if (config.delayErro < 0)
    {
        fprintf(stderr, "ERRO: DELAY_ERRO inválido (%d). Deve ser >= 0\n", config.delayErro);
        return 1;
    }
    if (config.maxLinha < 256)
    {
        fprintf(stderr, "ERRO: MAXLINE inválido (%d). Deve ser >= 256\n", config.maxLinha);
        return 1;
    }
    if (config.timeoutCliente < 0)
    {
        fprintf(stderr, "ERRO: TIMEOUT_CLIENTE não configurado ou inválido em %s\n", ficheiroConfig);
        fprintf(stderr, "-> Adicione: TIMEOUT_CLIENTE: 30 (por exemplo)\n");
        return 1;
    }

    // Validar configurações do Lobby
    if (config.maxClientesJogo <= 1)
    {
        fprintf(stderr, "ERRO: MAX_CLIENTES_JOGO inválido (%d) em %s\n", config.maxClientesJogo, ficheiroConfig);
        fprintf(stderr, "-> Deve ser > 1. Adicione: MAX_CLIENTES_JOGO: 10\n");
        return 1;
    }
    if (config.tempoAgregacao <= 0)
    {
        fprintf(stderr, "ERRO: TEMPO_AGREGACAO inválido (%d) em %s\n", config.tempoAgregacao, ficheiroConfig);
        fprintf(stderr, "-> Deve ser > 0. Adicione: TEMPO_AGREGACAO: 5\n");
        return 1;
    }

    // Validar configurações de modo
    if (config.modo != MODO_PADRAO && config.modo != MODO_DEBUG)
    {
        fprintf(stderr, "ERRO: MODO não configurado em %s\n", ficheiroConfig);
        fprintf(stderr, "-> Adicione: MODO: PADRAO ou MODO: DEBUG\n");
        return 1;
    }

    if (config.modo == MODO_PADRAO)
    {
        if (config.diasRetencaoLogs < 0)
        {
            fprintf(stderr, "ERRO: DIAS_RETENCAO_LOGS não configurado ou inválido em %s\n", ficheiroConfig);
            fprintf(stderr, "-> Adicione: DIAS_RETENCAO_LOGS: 7 (por exemplo)\n");
            return 1;
        }
    }

    if (config.modo == MODO_DEBUG)
    {
        if (config.limparLogsEncerramento < 0)
        {
            fprintf(stderr, "ERRO: LIMPAR_LOGS_ENCERRAMENTO não configurado em %s\n", ficheiroConfig);
            fprintf(stderr, "-> Adicione: LIMPAR_LOGS_ENCERRAMENTO: 1\n");
            return 1;
        }
    }

    // Guardar config em global para cleanup
    config_global = config;

    // Ajustar caminhos se necessário (tentar com e sem ../)
    // Usa o ficheiro de JOGOS como referência (sempre existe, ao contrário do log)
    FILE *test = fopen(config.ficheiroJogos, "r");
    if (!test)
    {
        // Tentar com ../
        char temp[256];
        snprintf(temp, sizeof(temp), "../%s", config.ficheiroJogos);
        test = fopen(temp, "r");
        if (test)
        {
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
    }
    else
    {
        fclose(test);
    }

    if (inicializarLog(config.ficheiroLog) != 0)
    {
        err_dump("Servidor: Falha a iniciar logs");
    }

    if (config.modo == MODO_PADRAO && config.diasRetencaoLogs > 0)
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "find logs/ -type f -name '*.log' -mtime +%d -delete 2>/dev/null", config.diasRetencaoLogs);
        system(cmd);
    }

    char log_init[512];
    snprintf(log_init, sizeof(log_init),
             "Servidor iniciado - Porta: %d, MaxFila: %d, MaxJogos: %d",
             config.porta, config.maxFila, config.maxJogos);
    registarEvento(0, EVT_SERVIDOR_INICIADO, log_init);

    // Registar handler para SIGINT (Ctrl+C) para cleanup gracioso
    signal(SIGINT, sigint_handler);

    // Evitar processos zombies - auto-reaping de processos filho
    signal(SIGCHLD, SIG_IGN);

    // Ignorar SIGPIPE (evita crash se cliente desconectar durante write)
    signal(SIGPIPE, SIG_IGN);

    atexit(cleanup_servidor);

    jogos = malloc(sizeof(Jogo) * config.maxJogos);
    jogos_global = jogos;
    if (!jogos)
    {
        registarEvento(0, EVT_ERRO_GERAL, "Falha ao alocar memória para jogos");
        err_dump("Servidor: Falha ao alocar memória para jogos");
    }

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

    DadosPartilhados *dados = mmap(NULL, sizeof(DadosPartilhados),
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (dados == MAP_FAILED)
    {
        free(jogos);
        jogos_global = NULL;
        err_dump("Erro ao criar memória partilhada");
    }

    dados_global = dados;

    // Inicializar estrutura do Lobby Dinâmico
    dados->numClientesJogando = 0; // Nenhum cliente jogando inicialmente
    dados->numClientesLobby = 0;   // Lobby vazio
    dados->numJogadoresAtivos = 0; // Nenhum jogador ativo
    dados->ultimaEntrada = 0;      // Sem entradas ainda
    dados->jogoAtual = -1;         // Nenhum jogo selecionado
    dados->jogoIniciado = 0;       // Jogo não iniciado
    dados->jogoTerminado = 0;      // Jogo não terminado
    dados->idVencedor = -1;        // Sem vencedor ainda
    dados->tempoVitoria = 0;       // Sem timestamp de vitória

    // Inicializa semáforos
    // O '1' no meio significa "partilhado entre processos"
    sem_init(&dados->mutex, 1, 1);          // Mutex começa a 1 (livre)
    sem_init(&dados->lobby_semaforo, 1, 0); // Semáforo do lobby começa a 0 (bloqueado)

    /* Cria socket stream (TCP) para Internet */
    printf("6. A criar socket TCP (AF_INET)...\n");
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        free(jogos);
        jogos_global = NULL;
        err_dump("Servidor: não foi possível abrir o socket stream");
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(config.porta);

    sockfd_global = sockfd;

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        err_dump("Servidor: não foi possível fazer bind");
    }

    listen(sockfd, config.maxFila);

    srand(time(NULL));
    numJogos_global = numJogos;

    if (pthread_create(&timer_thread, NULL, lobby_timer_thread, NULL) != 0)
    {
        err_dump("Servidor: erro ao criar thread de timer");
    }
    pthread_detach(timer_thread);

    printf("\n\033[1;36m");
    printf("╔══════════════════════════════════════╗\n");
    printf("║   SERVIDOR SUDOKU MULTIPLAYER       ║\n");
    printf("╚══════════════════════════════════════╝\033[0m\n");
    printf("\033[1mPorto:\033[0m %d | \033[1mJogos:\033[0m %d | \033[1mMax Clientes:\033[0m %d\n\n",
           config.porta, numJogos, config.maxClientesJogo);
    printf("\033[33m[Aguardando clientes...]\033[0m\n");

    for (;;)
    {
        clilen = sizeof(cli_addr);

        /* Aceita um novo cliente */
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
            err_dump("Servidor: erro no accept");

        char *ip_cliente = inet_ntoa(cli_addr.sin_addr);

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
            sou_processo_pai = 0; // Marcar como processo filho
            close(sockfd);        // Filho não precisa do socket "pai"

            // str_echo é a função do util-stream-server.c
            // É AQUI que vais implementar a lógica do protocolo.h
            str_echo(newsockfd, jogos, numJogos, dados, config.maxLinha, config.timeoutCliente);

            snprintf(log_init, sizeof(log_init),
                     "Cliente desconectado: %s", ip_cliente);
            registarEvento(0, EVT_CLIENTE_DESCONECTADO, log_init);
            exit(0);
        }

        /* PROCESSO PAI */
        close(newsockfd); // Pai não precisa do socket do cliente
    }

    // O cleanup é feito automaticamente por atexit() ou pelo signal handler
}
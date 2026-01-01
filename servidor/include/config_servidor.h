#ifndef CONFIG_SERVIDOR_H
#define CONFIG_SERVIDOR_H
#define MAX_JOGOS 100

typedef enum {
    MODO_PADRAO,  // Produção - logs organizados por data
    MODO_DEBUG    // Desenvolvimento - apaga logs ao encerrar
} ModoOperacao;

typedef struct {
    char ficheiroJogos[100];
    char ficheiroSolucoes[100];
    char ficheiroLog[100];
    int porta;                  // Porta do servidor
    int maxFila;                // Máximo de clientes em espera
    int maxJogos;               // Máximo de jogos a carregar
    int delayErro;              // Delay entre mensagens de erro (segundos)
    int maxLinha;               // Tamanho máximo de buffer de comunicação
    int timeoutCliente;         // Timeout para operações de socket com cliente (segundos)
    ModoOperacao modo;          // PADRAO ou DEBUG
    int diasRetencaoLogs;       // Dias para manter logs (modo PADRAO)
    int limparLogsEncerramento; // Apagar logs ao encerrar (modo DEBUG)
} ConfigServidor;

int lerConfigServidor(const char *nomeFicheiro, ConfigServidor *config);

typedef struct {
    int idjogo;
    char tabuleiro [82];
    char solucao[82];
} Jogo;

int carregarJogos(const char *ficheiro, Jogo jogos[], int maxJogos);

#endif

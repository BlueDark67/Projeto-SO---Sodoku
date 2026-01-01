#ifndef CONFIG_SERVIDOR_H
#define CONFIG_SERVIDOR_H
#define MAX_JOGOS 100

typedef struct {
    char ficheiroJogos[100];
    char ficheiroSolucoes[100];
    char ficheiroLog[100];
    int porta;                  // Porta do servidor
    int maxFila;                // Máximo de clientes em espera
    int maxJogos;               // Máximo de jogos a carregar
    int delayErro;              // Delay entre mensagens de erro (segundos)
    int maxLinha;               // Tamanho máximo de buffer de comunicação
} ConfigServidor;

int lerConfigServidor(const char *nomeFicheiro, ConfigServidor *config);

typedef struct {
    int idjogo;
    char tabuleiro [82];
    char solucao[82];
} Jogo;

int carregarJogos(const char *ficheiro, Jogo jogos[], int maxJogos);

#endif

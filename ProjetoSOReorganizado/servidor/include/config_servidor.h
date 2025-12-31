#ifndef CONFIG_SERVIDOR_H
#define CONFIG_SERVIDOR_H
#define MAX_JOGOS 100

typedef struct {
    char ficheiroJogos[100];
    char ficheiroSolucoes[100];
    char ficheiroLog[100];
} ConfigServidor;

int lerConfigServidor(const char *nomeFicheiro, ConfigServidor *config);

typedef struct {
    int idjogo;
    char tabuleiro [82];
    char solucao[82];
} Jogo;

int carregarJogos(const char *ficheiro, Jogo jogos[], int maxJogos);

#endif

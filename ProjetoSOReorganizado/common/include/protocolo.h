#ifndef PROTOCOLO_H
#define PROTOCOLO_H

typedef enum {
    PEDIR_JOGO = 1,      // C -> S
    ENVIAR_JOGO = 2,     // S -> C
    ENVIAR_SOLUCAO = 3,  // C -> S
    RESPOSTA_SOLUCAO = 4 // S -> C
} TipoMensagem;

typedef struct {
    TipoMensagem tipo;
    int idCliente;
    int idJogo;
    char tabuleiro[82]; // 81 chars + '\0'
    // Podes adicionar um campo para a resposta
    char resposta[50]; // "Certo", "Errado", etc.
} MensagemSudoku;

#endif
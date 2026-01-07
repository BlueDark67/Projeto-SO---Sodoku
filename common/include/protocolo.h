/*
 * common/include/protocolo.h
 *
 * Protocolo de Comunicação Cliente-Servidor
 *
 * Define as estruturas e tipos de mensagens trocadas entre
 * o cliente e o servidor no jogo Sudoku.
 *
 * Fluxo de Comunicação:
 * 1. Cliente -> Servidor: PEDIR_JOGO
 * 2. Servidor -> Cliente: ENVIAR_JOGO (com tabuleiro)
 * 3. Cliente -> Servidor: ENVIAR_SOLUCAO (com tabuleiro resolvido)
 * 4. Servidor -> Cliente: RESPOSTA_SOLUCAO (resultado da verificação)
 *
 * Todas as mensagens usam a estrutura MensagemSudoku que contém:
 * - Tipo de mensagem
 * - IDs de cliente e jogo
 * - Tabuleiro (81 células + terminador)
 * - Campo de resposta (para resultados)
 */

#ifndef PROTOCOLO_H
#define PROTOCOLO_H

typedef enum
{
    PEDIR_JOGO = 1,       // Cliente pede um jogo ao servidor
    ENVIAR_JOGO = 2,      // Servidor envia jogo ao cliente
    ENVIAR_SOLUCAO = 3,   // Cliente envia solução ao servidor
    RESPOSTA_SOLUCAO = 4, // Servidor responde com verificação
    VALIDAR_BLOCO = 5,    // Cliente pede validação de um bloco 3x3
    RESPOSTA_BLOCO = 6,   // Servidor responde sobre o bloco
    JOGO_TERMINADO = 7    // Servidor informa que jogo acabou (alguém ganhou)
} TipoMensagem;

typedef struct
{
    TipoMensagem tipo;     // Tipo da mensagem (ver enum acima)
    int idCliente;         // ID do cliente que envia/recebe
    int idJogo;            // ID do jogo Sudoku
    char tabuleiro[82];    // Tabuleiro 9x9 (81 dígitos + '\0')
    char resposta[50];     // Resposta do servidor ("Correto", "Incorreto", etc.)
    int bloco_id;          // ID do bloco (0-8) para validação parcial
    int conteudo_bloco[9]; // Conteúdo do bloco para validação
} MensagemSudoku;

#endif
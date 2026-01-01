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

/**
 * @brief Tipos de mensagens do protocolo
 * 
 * Cada mensagem trocada entre cliente e servidor tem um destes tipos
 */
typedef enum {
    PEDIR_JOGO = 1,      // Cliente pede um jogo ao servidor
    ENVIAR_JOGO = 2,     // Servidor envia jogo ao cliente
    ENVIAR_SOLUCAO = 3,  // Cliente envia solução ao servidor
    RESPOSTA_SOLUCAO = 4 // Servidor responde com verificação
} TipoMensagem;

/**
 * @brief Estrutura de mensagem do protocolo
 * 
 * Usada para todas as comunicações entre cliente e servidor.
 * Campos não usados em cada tipo de mensagem são ignorados.
 */
typedef struct {
    TipoMensagem tipo;  // Tipo da mensagem (ver enum acima)
    int idCliente;      // ID do cliente que envia/recebe
    int idJogo;         // ID do jogo Sudoku
    char tabuleiro[82]; // Tabuleiro 9x9 (81 dígitos + '\0')
    char resposta[50];  // Resposta do servidor ("Correto", "Incorreto", etc.)
} MensagemSudoku;

#endif
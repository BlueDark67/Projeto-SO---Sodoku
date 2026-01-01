// util.h
#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// ... (mantenha as suas declarações readn/writen/etc antigas) ...

extern int readn(int fd, char *ptr, int nbytes);
extern int writen(int fd, char *ptr, int nbytes);
extern int readline(int fd, char *ptr, int maxlen);
extern void err_dump(char *msg);

/**
 * @brief Imprime uma mensagem de erro formatada no stderr
 * Ex: erro("Falha ao abrir %s", ficheiro); -> "[ERRO] Falha ao abrir config.txt"
 */
void erro(const char *fmt, ...);

/**
 * @brief Imprime uma mensagem de aviso formatada no stderr
 * Ex: aviso("Tentativa %d falhou", i); -> "[AVISO] Tentativa 1 falhou"
 */
void aviso(const char *fmt, ...);

/**
 * @brief Lê um número inteiro do stdin de forma robusta
 * 
 * @param prompt Mensagem a mostrar ao utilizador
 * @param min Valor mínimo aceitável
 * @param max Valor máximo aceitável
 * @return O valor inteiro lido e validado
 */
int lerInteiro(const char *prompt, int min, int max);

/**
 * @brief Ajusta o caminho de um ficheiro verificando se existe localmente ou na diretoria pai
 * 
 * Útil para executar o programa tanto da raiz como da pasta build/
 * 
 * @param caminhoOriginal Caminho base do ficheiro (ex: "config/file.conf")
 * @param bufferDestino Buffer onde será escrito o caminho resolvido
 * @param tamanhoBuffer Tamanho do buffer de destino
 * @return 0 se encontrou o ficheiro (caminho ajustado em bufferDestino), -1 se não encontrou
 */
int ajustarCaminho(const char *caminhoOriginal, char *bufferDestino, size_t tamanhoBuffer);

/**
 * @brief Limpa o ecrã do terminal de forma portável (ANSI escape codes)
 */
void limparEcra(void);

#endif
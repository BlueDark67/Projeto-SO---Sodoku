#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern int readn(int fd, char *ptr, int nbytes);
extern int writen(int fd, char *ptr, int nbytes);
extern int readline(int fd, char *ptr, int maxlen);
extern void err_dump(char *msg);

void erro(const char *fmt, ...);

void aviso(const char *fmt, ...);

int lerInteiro(const char *prompt, int min, int max);

int ajustarCaminho(const char *caminhoOriginal, char *bufferDestino, size_t tamanhoBuffer);

void limparEcra(void);

#endif
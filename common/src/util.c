#include <stdio.h>
#include <errno.h>
#include "util.h"
#include <unistd.h>  // Para as funções read() e write()
#include <stdlib.h>  // Para a função exit() (usada em err_dump)
#include <stdarg.h>  // Para va_list, va_start, va_end

/* Funções utilitárias retiradas de "UNIX Networking Programming" */

/* Lê nbytes de um ficheiro/socket.
   Bloqueia até conseguir ler os nbytes ou dar erro */
int readn(int fd, char *ptr, int nbytes)
{
    int nleft, nread;

    nleft = nbytes;
    while (nleft > 0)
    {
        nread = read(fd, ptr, nleft);
        if (nread < 0)
            return (nread);
        else if (nread == 0)
            break;

        nleft -= nread;
        ptr += nread;
    }
    return (nbytes - nleft);
}

/* Escreve nbytes num ficheiro/socket.
   Bloqueia até conseguir escrever os nbytes ou dar erro */
int writen(int fd, char *ptr, int nbytes)
{
    int nleft, nwritten;

    nleft = nbytes;
    while (nleft > 0)
    {
        nwritten = write(fd, ptr, nleft);
        if (nwritten <= 0)
            return (nwritten);

        nleft -= nwritten;
        ptr += nwritten;
    }
    return (nbytes - nleft);
}

/* Lê uma linha de um ficheiro/socket (até \n, maxlen ou \0).
   Bloqueia até ler a linha ou dar erro.
   Retorna quantos caracteres conseguiu ler */
int readline(int fd, char *ptr, int maxlen)
{
    int n, rc;
    char c;

    for (n = 1; n < maxlen; n++)
    {
        if ((rc = read(fd, &c, 1)) == 1)
        {
            *ptr++ = c;
            if (c == '\n')
                break;
        }
        else if (rc == 0)
        {
            if (n == 1)
                return (0);
            else
                break;
        }
        else
            return (-1);
    }

    /* Não esquecer de terminar a string */
    *ptr = 0;

    /* Note-se que n foi incrementado de modo a contar
       com o \n ou \0 */
    return (n);
}

/* Mensagem de erro */
void err_dump(char *msg)
{
    perror(msg);
    exit(1);
}
/* Verifica se ficheiro existe e ajusta caminho se necessário (suporte build/) */
int ajustarCaminho(const char *caminhoOriginal, char *bufferDestino, size_t tamanhoBuffer) {
    // 1. Tentar caminho original (execução da raiz)
    if (access(caminhoOriginal, F_OK) == 0) {
        snprintf(bufferDestino, tamanhoBuffer, "%s", caminhoOriginal);
        return 0;
    }
    
    // 2. Tentar com prefixo ../ (execução de build/)
    char caminhoAlternativo[512];
    snprintf(caminhoAlternativo, sizeof(caminhoAlternativo), "../%s", caminhoOriginal);
    
    if (access(caminhoAlternativo, F_OK) == 0) {
        snprintf(bufferDestino, tamanhoBuffer, "%s", caminhoAlternativo);
        return 0;
    }
    
    // 3. Não encontrado
    return -1;
}

/* Limpa o ecrã usando códigos ANSI */
void limparEcra(void) {
    printf("\033[H\033[J");
    fflush(stdout);
}

/* Imprime mensagem de erro padronizada */
void erro(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "\033[1;31m[ERRO]\033[0m "); // Vermelho
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

/* Imprime mensagem de aviso padronizada */
void aviso(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "\033[1;33m[AVISO]\033[0m "); // Amarelo
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

/* Lê um inteiro de forma robusta (proteção contra buffer overflow e input inválido) */
int lerInteiro(const char *prompt, int min, int max) {
    char buffer[128];
    int valor;
    int valido = 0;

    while (!valido) {
        if (prompt) printf("%s", prompt);
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            return min; // EOF ou erro
        }

        // Tentar converter
        char *endptr;
        long num = strtol(buffer, &endptr, 10);

        // Verificar se foi lido algo e se o resto da linha é espaço/newline
        if (endptr == buffer) {
            printf("Entrada inválida. Por favor insira um número.\n");
            continue;
        }
        
        // Verificar limites
        if (num < min || num > max) {
            printf("Por favor insira um valor entre %d e %d.\n", min, max);
            continue;
        }

        valor = (int)num;
        valido = 1;
    }
    return valor;
}

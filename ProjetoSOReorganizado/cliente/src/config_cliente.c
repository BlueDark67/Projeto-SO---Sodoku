/*
 * cliente/src/config_cliente.c
 * 
 * Gestão de Configurações do Cliente
 * 
 * Este módulo lê configurações de ficheiros .conf para o cliente
 * 
 * Parâmetros suportados:
 * - IP_SERVIDOR: Endereço IP do servidor (formato: xxx.xxx.xxx.xxx)
 * - PORTA: Porta TCP do servidor
 * - ID_CLIENTE: Identificador único deste cliente
 * - LOG: Caminho para ficheiro de log do cliente
 * 
 * Formato do ficheiro .conf:
 * PARAMETRO: valor
 * Linhas começadas por # são comentários
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "config_cliente.h"

#define MAX_LINHA 256

/**
 * @brief Remove espaços em branco (como ' ' ou '\n') do início e fim de uma string.
 */
static void trim(char *str) {
    char *fim;

    // Remove espaços do início
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) // String vazia ou só com espaços
        return;

    // Remove espaços do fim
    fim = str + strlen(str) - 1;
    while (fim > str && isspace((unsigned char)*fim)) fim--;

    // Escreve o novo terminador nulo
    *(fim + 1) = '\0';
}

/**
 * @brief Lê e valida o ficheiro de configuração do cliente
 * 
 * Carrega configurações de conexão e identificação do cliente
 * 
 * @param nomeFicheiro Caminho para o ficheiro .conf
 * @param config Estrutura onde as configurações serão armazenadas
 * @return 0 em sucesso, -1 em erro
 */
int lerConfigCliente(const char *nomeFicheiro, ConfigCliente *config) {
    FILE *f = fopen(nomeFicheiro, "r");
    if (!f) {
        perror("Erro ao abrir ficheiro de configuração do cliente");
        return -1;
    }

    char linha[MAX_LINHA];
    int linha_num = 0;

    // Inicializar com valores inválidos para detetar parâmetros em falta
    // Se ficarem com estes valores, significa que não foram configurados
    config->idCliente = -1;
    config->porta = -1;
    config->ipServidor[0] = '\0';
    config->ficheiroLog[0] = '\0';

    // Processar cada linha do ficheiro

    while (fgets(linha, sizeof(linha), f)) {
        linha_num++;
        char chave[100], valor[100];

        // Remove newline do fim
        linha[strcspn(linha, "\n")] = 0;

        // Ignora linhas vazias ou comentários
        if (linha[0] == '\0' || linha[0] == '#') {
            continue;
        }

        // Procura o separador ':'
        char *separador = strchr(linha, ':');
        if (!separador) {
            printf("Aviso: Formato inválido na linha %d do config cliente: %s\n", linha_num, linha);
            continue;
        }

        // Separa a chave
        *separador = '\0'; // Corta a string no ':'
        strncpy(chave, linha, sizeof(chave) - 1);
        trim(chave);

        // Separa o valor
        // Separa o valor
        strncpy(valor, separador + 1, sizeof(valor) - 1);

        // --- Início do NOVO código de limpeza ---
        char *valor_limpo = valor;

        // 1. Remove espaços do início
        while (isspace((unsigned char)*valor_limpo)) {
            valor_limpo++;
        }

        // 2. Remove espaços do fim
        char *fim = valor_limpo + strlen(valor_limpo) - 1;
        while (fim > valor_limpo && isspace((unsigned char)*fim)) {
            fim--;
        }
        *(fim + 1) = '\0'; // Escreve o novo fim da string
        // --- Fim do NOVO código de limpeza ---

        // Atribui os valores à estrutura
        if (strcmp(chave, "IP_SERVIDOR") == 0) {
            strncpy(config->ipServidor, valor_limpo, sizeof(config->ipServidor) - 1);
        } else if (strcmp(chave, "ID_CLIENTE") == 0) {
            config->idCliente = atoi(valor);
        } else if (strcmp(chave, "PORTA") == 0) {
            config->porta = atoi(valor);
        } else if (strcmp(chave, "LOG") == 0) {
            strncpy(config->ficheiroLog, valor_limpo, sizeof(config->ficheiroLog) - 1);
        }
    }

    fclose(f);
    return 0;
}
/*
 * servidor/src/config_servidor.c
 * 
 * Gestão de Configurações do Servidor
 * 
 * Este módulo lê e valida configurações de ficheiros .conf
 * 
 * Parâmetros suportados:
 * - PORTA: Porta TCP para o servidor escutar
 * - MAX_FILA: Número máximo de conexões em espera
 * - MAX_JOGOS: Capacidade máxima de jogos a carregar
 * - DELAY_ERRO: Segundos de espera após erro (anticheat)
 * - MAXLINE: Tamanho máximo do buffer de comunicação
 * - JOGOS: Caminho para ficheiro de jogos
 * - LOG: Caminho para ficheiro de log
 * 
 * Formato do ficheiro .conf:
 * PARAMETRO: valor
 * Linhas começadas por # são comentários
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config_servidor.h"

/**
 * @brief Remove espaços e caracteres de controle do final de uma string
 * @param str String a processar (modificada in-place)
 */
static void trim(char *str) {
    char *end = str + strlen(str) - 1;
    while (end >= str && (isspace((unsigned char)*end) || *end == '\r')) {
        end--;
    }
    *(end + 1) = '\0';
}

/**
 * @brief Lê e valida o ficheiro de configuração do servidor
 * 
 * Carrega todas as configurações de um ficheiro .conf e valida
 * que todos os parâmetros obrigatórios estão presentes
 * 
 * @param nomeFicheiro Caminho para o ficheiro .conf
 * @param config Estrutura onde as configurações serão armazenadas
 * @return 0 em sucesso, -1 em erro
 */
int lerConfigServidor(const char *nomeFicheiro, ConfigServidor *config) {
    FILE *f = fopen(nomeFicheiro, "r");
    if (!f) {
        printf("Erro ao abrir o ficheiro de configuração: %s\n", nomeFicheiro);
        return -1;
    }

    // Inicializar com valores inválidos para forçar configuração explícita
    config->porta = -1;
    config->maxFila = -1;
    config->maxJogos = -1;
    config->delayErro = -1;
    config->maxLinha = -1;
    config->ficheiroJogos[0] = '\0';
    config->ficheiroSolucoes[0] = '\0';
    config->ficheiroLog[0] = '\0';
    config->modo = -1;  // Inválido - deve ser configurado explicitamente
    config->diasRetencaoLogs = -1;  // Inválido
    config->limparLogsEncerramento = -1;  // Inválido

    char linha[200];
    while (fgets(linha, sizeof(linha), f)) {
        // Ignorar linhas vazias e comentários (começam com #)
        if (linha[0] == '\n' || linha[0] == '#') {
            continue;
        }
        
        // Parse formato: PARAMETRO: valor
        char parametro[50], valor[150];
        if (sscanf(linha, "%49[^:]: %149[^\n]", parametro, valor) == 2) {
            trim(valor); // Limpar espaços e \r do final
            
            // Mapear cada parâmetro para o campo correspondente
            if (strcmp(parametro, "JOGOS") == 0) {
                if (strlen(valor) >= sizeof(config->ficheiroJogos)) {
                    fprintf(stderr, "ERRO: Valor de JOGOS muito longo (máx %zu chars)\n", 
                            sizeof(config->ficheiroJogos) - 1);
                    fclose(f);
                    return -1;
                }
                strncpy(config->ficheiroJogos, valor, sizeof(config->ficheiroJogos) - 1);
                config->ficheiroJogos[sizeof(config->ficheiroJogos) - 1] = '\0';
            } else if (strcmp(parametro, "SOLUCOES") == 0) {
                if (strlen(valor) >= sizeof(config->ficheiroSolucoes)) {
                    fprintf(stderr, "ERRO: Valor de SOLUCOES muito longo (máx %zu chars)\n", 
                            sizeof(config->ficheiroSolucoes) - 1);
                    fclose(f);
                    return -1;
                }
                strncpy(config->ficheiroSolucoes, valor, sizeof(config->ficheiroSolucoes) - 1);
                config->ficheiroSolucoes[sizeof(config->ficheiroSolucoes) - 1] = '\0';
            } else if (strcmp(parametro, "LOG") == 0) {
                if (strlen(valor) >= sizeof(config->ficheiroLog)) {
                    fprintf(stderr, "ERRO: Valor de LOG muito longo (máx %zu chars)\n", 
                            sizeof(config->ficheiroLog) - 1);
                    fclose(f);
                    return -1;
                }
                strncpy(config->ficheiroLog, valor, sizeof(config->ficheiroLog) - 1);
                config->ficheiroLog[sizeof(config->ficheiroLog) - 1] = '\0';
            } else if (strcmp(parametro, "PORTA") == 0) {
                config->porta = atoi(valor);
            } else if (strcmp(parametro, "MAX_FILA") == 0) {
                config->maxFila = atoi(valor);
            } else if (strcmp(parametro, "MAX_JOGOS") == 0) {
                config->maxJogos = atoi(valor);
            } else if (strcmp(parametro, "DELAY_ERRO") == 0) {
                config->delayErro = atoi(valor);
            } else if (strcmp(parametro, "MAXLINE") == 0) {
                config->maxLinha = atoi(valor);
            } else if (strcmp(parametro, "MODO") == 0) {
                if (strcmp(valor, "DEBUG") == 0) {
                    config->modo = MODO_DEBUG;
                } else if (strcmp(valor, "PADRAO") == 0) {
                    config->modo = MODO_PADRAO;
                }
                // Se não for nenhum dos dois, fica -1 (inválido)
            } else if (strcmp(parametro, "DIAS_RETENCAO_LOGS") == 0) {
                config->diasRetencaoLogs = atoi(valor);
            } else if (strcmp(parametro, "LIMPAR_LOGS_ENCERRAMENTO") == 0) {
                config->limparLogsEncerramento = atoi(valor);
            }
        }
    }

    fclose(f);
    return 0;
}

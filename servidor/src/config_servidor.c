// servidor/src/config_servidor.c - Gestão de configurações do servidor

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config_servidor.h"

static void trim(char *str)
{
    char *end = str + strlen(str) - 1;
    while (end >= str && (isspace((unsigned char)*end) || *end == '\r'))
    {
        end--;
    }
    *(end + 1) = '\0';
}

// Lê e valida o ficheiro de configuração do servidor
int lerConfigServidor(const char *nomeFicheiro, ConfigServidor *config)
{
    FILE *f = fopen(nomeFicheiro, "r");
    if (!f)
    {
        printf("Erro ao abrir o ficheiro de configuração: %s\n", nomeFicheiro);
        return -1;
    }

    config->porta = -1;
    config->maxFila = -1;
    config->maxJogos = -1;
    config->delayErro = -1;
    config->maxLinha = -1;
    config->timeoutCliente = -1;
    config->maxClientesJogo = -1;
    config->tempoAgregacao = -1;
    config->ficheiroJogos[0] = '\0';
    config->ficheiroSolucoes[0] = '\0';
    config->ficheiroLog[0] = '\0';
    config->modo = -1;
    config->diasRetencaoLogs = -1;
    config->limparLogsEncerramento = -1;

    char linha[200];
    while (fgets(linha, sizeof(linha), f))
    {
        if (linha[0] == '\n' || linha[0] == '#')
        {
            continue;
        }

        char parametro[50], valor[150];
        if (sscanf(linha, "%49[^:]: %149[^\n]", parametro, valor) == 2)
        {
            trim(valor);

            // Mapear cada parâmetro para o campo correspondente
            if (strcmp(parametro, "JOGOS") == 0)
            {
                if (strlen(valor) >= sizeof(config->ficheiroJogos))
                {
                    fprintf(stderr, "ERRO: Valor de JOGOS muito longo (máx %zu chars)\n",
                            sizeof(config->ficheiroJogos) - 1);
                    fclose(f);
                    return -1;
                }
                strncpy(config->ficheiroJogos, valor, sizeof(config->ficheiroJogos) - 1);
                config->ficheiroJogos[sizeof(config->ficheiroJogos) - 1] = '\0';
            }
            else if (strcmp(parametro, "SOLUCOES") == 0)
            {
                if (strlen(valor) >= sizeof(config->ficheiroSolucoes))
                {
                    fprintf(stderr, "ERRO: Valor de SOLUCOES muito longo (máx %zu chars)\n",
                            sizeof(config->ficheiroSolucoes) - 1);
                    fclose(f);
                    return -1;
                }
                strncpy(config->ficheiroSolucoes, valor, sizeof(config->ficheiroSolucoes) - 1);
                config->ficheiroSolucoes[sizeof(config->ficheiroSolucoes) - 1] = '\0';
            }
            else if (strcmp(parametro, "LOG") == 0)
            {
                if (strlen(valor) >= sizeof(config->ficheiroLog))
                {
                    fprintf(stderr, "ERRO: Valor de LOG muito longo (máx %zu chars)\n",
                            sizeof(config->ficheiroLog) - 1);
                    fclose(f);
                    return -1;
                }
                strncpy(config->ficheiroLog, valor, sizeof(config->ficheiroLog) - 1);
                config->ficheiroLog[sizeof(config->ficheiroLog) - 1] = '\0';
            }
            else if (strcmp(parametro, "PORTA") == 0)
            {
                config->porta = atoi(valor);
            }
            else if (strcmp(parametro, "MAX_FILA") == 0)
            {
                config->maxFila = atoi(valor);
            }
            else if (strcmp(parametro, "MAX_JOGOS") == 0)
            {
                config->maxJogos = atoi(valor);
            }
            else if (strcmp(parametro, "DELAY_ERRO") == 0)
            {
                config->delayErro = atoi(valor);
            }
            else if (strcmp(parametro, "MAXLINE") == 0)
            {
                config->maxLinha = atoi(valor);
            }
            else if (strcmp(parametro, "TIMEOUT_CLIENTE") == 0)
            {
                config->timeoutCliente = atoi(valor);
            }
            else if (strcmp(parametro, "MAX_CLIENTES_JOGO") == 0)
            {
                config->maxClientesJogo = atoi(valor);
            }
            else if (strcmp(parametro, "TEMPO_AGREGACAO") == 0)
            {
                config->tempoAgregacao = atoi(valor);
            }
            else if (strcmp(parametro, "MODO") == 0)
            {
                if (strcmp(valor, "DEBUG") == 0)
                {
                    config->modo = MODO_DEBUG;
                }
                else if (strcmp(valor, "PADRAO") == 0)
                {
                    config->modo = MODO_PADRAO;
                }
                // Se não for nenhum dos dois, fica -1 (inválido)
            }
            else if (strcmp(parametro, "DIAS_RETENCAO_LOGS") == 0)
            {
                config->diasRetencaoLogs = atoi(valor);
            }
            else if (strcmp(parametro, "LIMPAR_LOGS_ENCERRAMENTO") == 0)
            {
                config->limparLogsEncerramento = atoi(valor);
            }
        }
    }

    fclose(f);
    return 0;
}

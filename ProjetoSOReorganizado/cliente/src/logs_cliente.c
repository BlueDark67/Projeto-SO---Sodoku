#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <libgen.h>
#include "logs_cliente.h"

static FILE *ficheiro_log_cliente = NULL;
static int id_cliente_global = 0;

// Cria diretórios recursivamente
static int criar_diretorios_cliente(const char *path) {
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
    return 0;
}

int inicializarLogCliente(const char *ficheiroLog, int idCliente) {
    id_cliente_global = idCliente;
    
    // Extrair o diretório do caminho do ficheiro
    char *caminho_copia = strdup(ficheiroLog);
    char *dir = dirname(caminho_copia);
    
    // Verificar se o diretório existe, se não criar
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        printf("Diretório de logs não existe. A criar: %s\n", dir);
        criar_diretorios_cliente(dir);
    }
    free(caminho_copia);
    
    // Abrir/criar o ficheiro de log
    ficheiro_log_cliente = fopen(ficheiroLog, "a");
    if (!ficheiro_log_cliente) {
        printf("Erro ao abrir ficheiro de log do cliente: %s\n", ficheiroLog);
        return -1;
    }
    
    // Escrever cabeçalho se o ficheiro estiver vazio
    fseek(ficheiro_log_cliente, 0, SEEK_END);
    long size = ftell(ficheiro_log_cliente);
    
    if (size == 0) {
        fprintf(ficheiro_log_cliente, "==========================================================================\n");
        fprintf(ficheiro_log_cliente, " LOG DO CLIENTE #%d - SUDOKU\n", idCliente);
        fprintf(ficheiro_log_cliente, "==========================================================================\n");
        fprintf(ficheiro_log_cliente, "%-19s %-12s %s\n", "Data/Hora", "Evento", "Descrição");
        fprintf(ficheiro_log_cliente, "%-19s %-12s %s\n", "-------------------", "------------", "------------------------------------");
    }
    
    fflush(ficheiro_log_cliente);
    return 0;
}

void registarEventoCliente(CodigoEventoCliente evento, const char *descricao) {
    if (!ficheiro_log_cliente) {
        printf("AVISO: Sistema de log do cliente não inicializado!\n");
        return;
    }
    
    time_t agora;
    struct tm *info_tempo;
    char buffer_tempo[64];
    
    time(&agora);
    info_tempo = localtime(&agora);
    strftime(buffer_tempo, sizeof(buffer_tempo), "%Y-%m-%d %H:%M:%S", info_tempo);
    
    const char *nome_evento = "";
    switch(evento) {
        case EVTC_CLIENTE_INICIADO: nome_evento = "INÍCIO"; break;
        case EVTC_CONEXAO_ESTABELECIDA: nome_evento = "CONEXÃO"; break;
        case EVTC_JOGO_RECEBIDO: nome_evento = "JOGO RX"; break;
        case EVTC_SOLUCAO_ENVIADA: nome_evento = "SOL TX"; break;
        case EVTC_RESULTADO_RECEBIDO: nome_evento = "RESULT"; break;
        case EVTC_SOLUCAO_CORRETA: nome_evento = "✓ CERTO"; break;
        case EVTC_SOLUCAO_INCORRETA: nome_evento = "✗ ERRADO"; break;
        case EVTC_NOVO_JOGO_PEDIDO: nome_evento = "PEDIDO"; break;
        case EVTC_CONEXAO_FECHADA: nome_evento = "FECHADO"; break;
        case EVTC_ERRO: nome_evento = "ERRO"; break;
        default: nome_evento = "?"; break;
    }
    
    fprintf(ficheiro_log_cliente, "%-19s %-12s %s\n", 
            buffer_tempo, nome_evento, descricao ? descricao : "");
    fflush(ficheiro_log_cliente);
}

void fecharLogCliente() {
    if (ficheiro_log_cliente) {
        registarEventoCliente(EVTC_CONEXAO_FECHADA, "Cliente encerrado");
        fclose(ficheiro_log_cliente);
        ficheiro_log_cliente = NULL;
    }
}

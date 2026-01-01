#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <libgen.h>
#include "logs.h"

static FILE *ficheiro_log = NULL;

// Cria diretórios recursivamente
static int criar_diretorios(const char *path) {
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

int inicializarLog(const char *ficheiroLog) {
    // Extrair o diretório do caminho do ficheiro
    char *caminho_copia = strdup(ficheiroLog);
    char *dir = dirname(caminho_copia);
    
    // Verificar se o diretório existe, se não criar
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        printf("Diretório de logs não existe. A criar: %s\n", dir);
        criar_diretorios(dir);
    }
    free(caminho_copia);
    
    // Abrir/criar o ficheiro de log
    ficheiro_log = fopen(ficheiroLog, "a");
    if (!ficheiro_log) {
        printf("Erro ao abrir ficheiro de log: %s\n", ficheiroLog);
        return -1;
    }
    
    // Escrever cabeçalho se o ficheiro estiver vazio
    fseek(ficheiro_log, 0, SEEK_END);
    long size = ftell(ficheiro_log);
    
    if (size == 0) {
        fprintf(ficheiro_log, "%-12s %-8s %-18s %s\n", "IdUtilizador", "Hora", "Acontecimento", "Descrição");
        fprintf(ficheiro_log, "%-12s %-8s %-18s %s\n", "============", "========", "==================", "===========");
    }
    
    fflush(ficheiro_log);
    return 0;
}

void registarEvento(int idUtilizador, CodigoEvento evento, const char *descricao) {
    if (!ficheiro_log) {
        printf("AVISO: Sistema de log não inicializado!\n");
        return;
    }
    
    time_t agora;
    struct tm *info_tempo;
    char buffer_tempo[20];
    
    time(&agora);
    info_tempo = localtime(&agora);
    strftime(buffer_tempo, sizeof(buffer_tempo), "%H:%M:%S", info_tempo);
    
    char id_str[13];
    if (idUtilizador == 0) {
        snprintf(id_str, sizeof(id_str), "[Servidor]");
    } else {
        snprintf(id_str, sizeof(id_str), "%d", idUtilizador);
    }
    
    fprintf(ficheiro_log, "%-12s %-8s %-18s %s\n", 
            id_str, buffer_tempo, obterDescricaoEvento(evento), descricao ? descricao : "");
    fflush(ficheiro_log);
    
    // Também imprimir no console para debug
    printf("[LOG] [%s] User:%d Event:%d - %s\n", 
           buffer_tempo, idUtilizador, evento, descricao ? descricao : "");
}

void fecharLog() {
    if (ficheiro_log) {
        registarEvento(0, EVT_SERVIDOR_INICIADO, "Servidor encerrado");
        fclose(ficheiro_log);
        ficheiro_log = NULL;
    }
}

const char* obterDescricaoEvento(CodigoEvento evento) {
    switch(evento) {
        case EVT_SERVIDOR_INICIADO: return "Servidor Iniciado";
        case EVT_JOGOS_CARREGADOS: return "Jogos Carregados";
        case EVT_CLIENTE_CONECTADO: return "Cliente Conectado";
        case EVT_CLIENTE_DESCONECTADO: return "Cliente Desconectado";
        case EVT_JOGO_PEDIDO: return "Jogo Pedido";
        case EVT_JOGO_ENVIADO: return "Jogo Enviado";
        case EVT_SOLUCAO_RECEBIDA: return "Solucao Recebida";
        case EVT_SOLUCAO_VERIFICADA: return "Solucao Verificada";
        case EVT_SOLUCAO_CORRETA: return "Solucao Correta";
        case EVT_SOLUCAO_ERRADA: return "Solucao Errada";
        case EVT_ERRO_GERAL: return "Erro Geral";
        default: return "Evento Desconhecido";
    }
}
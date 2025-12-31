#include <stdio.h>
#include <time.h>
#include <string.h>
#include "logs.h"

static FILE *ficheiro_log = NULL;

int inicializarLog(const char *ficheiroLog) {
    ficheiro_log = fopen(ficheiroLog, "a");
    if (!ficheiro_log) {
        printf("Erro ao abrir ficheiro de log: %s\n", ficheiroLog);
        return -1;
    }
    
    // Escrever cabeçalho se o ficheiro estiver vazio
    fseek(ficheiro_log, 0, SEEK_END);
    long size = ftell(ficheiro_log);
    
    if (size == 0) {
        fprintf(ficheiro_log, "IdUtilizador\tHora\t\tAcontecimento\tDescricao\n");
        fprintf(ficheiro_log, "============\t========\t=============\t=========\n");
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
    
    fprintf(ficheiro_log, "%d\t\t%s\t%d\t\t\"%s\"\n", 
            idUtilizador, buffer_tempo, evento, descricao ? descricao : "");
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
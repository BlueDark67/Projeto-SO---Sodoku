#ifndef LOGS_H
#define LOGS_H

// Códigos de eventos
typedef enum {
    EVT_SERVIDOR_INICIADO = 1,
    EVT_JOGOS_CARREGADOS = 2,
    EVT_CLIENTE_CONECTADO = 3,
    EVT_CLIENTE_DESCONECTADO = 4,
    EVT_JOGO_PEDIDO = 5,
    EVT_JOGO_ENVIADO = 6,
    EVT_SOLUCAO_RECEBIDA = 7,
    EVT_SOLUCAO_VERIFICADA = 8,
    EVT_SOLUCAO_CORRETA = 9,
    EVT_SOLUCAO_ERRADA = 10,
    EVT_ERRO_GERAL = 99
} CodigoEvento;

// Inicializa o sistema de logs
int inicializarLog(const char *ficheiroLog);

// Registar um evento no log
void registarEvento(int idUtilizador, CodigoEvento evento, const char *descricao);

// Fechar o ficheiro de log
void fecharLog();

// Obter descrição do evento
const char* obterDescricaoEvento(CodigoEvento evento);

#endif
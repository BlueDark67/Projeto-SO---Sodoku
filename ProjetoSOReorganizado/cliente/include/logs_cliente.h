#ifndef LOGS_CLIENTE_H
#define LOGS_CLIENTE_H

// Códigos de eventos do cliente
typedef enum {
    EVTC_CLIENTE_INICIADO = 1,
    EVTC_CONEXAO_ESTABELECIDA = 2,
    EVTC_JOGO_RECEBIDO = 3,
    EVTC_SOLUCAO_ENVIADA = 4,
    EVTC_RESULTADO_RECEBIDO = 5,
    EVTC_SOLUCAO_CORRETA = 6,
    EVTC_SOLUCAO_INCORRETA = 7,
    EVTC_NOVO_JOGO_PEDIDO = 8,
    EVTC_CONEXAO_FECHADA = 9,
    EVTC_ERRO = 99
} CodigoEventoCliente;

// Inicializa o sistema de logs do cliente
int inicializarLogCliente(const char *ficheiroLog, int idCliente);

// Registar um evento no log do cliente
void registarEventoCliente(CodigoEventoCliente evento, const char *descricao);

// Fechar o ficheiro de log do cliente
void fecharLogCliente();

#endif

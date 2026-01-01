/*
 * cliente/src/logs_cliente.c
 * 
 * Sistema de Logging do Cliente
 * 
 * Regista a perspetiva do cliente nas interações:
 * - Início do cliente
 * - Conexão ao servidor
 * - Jogos recebidos (com contagem de células)
 * - Soluções enviadas (com tempo de resolução)
 * - Resultados (correto/incorreto)
 * - Novos pedidos
 * - Encerramento
 * 
 * Formato do log:
 * Data/Hora | Evento | Descrição
 * 
 * Eventos suportados (CodigoEventoCliente):
 * - EVTC_CLIENTE_INICIADO: Início da execução
 * - EVTC_CONEXAO_ESTABELECIDA: Ligação ao servidor
 * - EVTC_JOGO_RECEBIDO: Recebeu jogo do servidor
 * - EVTC_SOLUCAO_ENVIADA: Enviou solução
 * - EVTC_RESULTADO_RECEBIDO: Recebeu verificação
 * - EVTC_SOLUCAO_CORRETA: Acertou
 * - EVTC_SOLUCAO_INCORRETA: Errou
 * - EVTC_NOVO_JOGO_PEDIDO: Pediu novo jogo
 * - EVTC_CONEXAO_FECHADA: Desconectou
 * - EVTC_ERRO: Erro qualquer
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>   // Para flock() (file locking)
#include <stdlib.h>
#include <libgen.h>
#include "logs_cliente.h"

static FILE *ficheiro_log_cliente = NULL;
static int id_cliente_global = 0;

/**
 * @brief Cria diretórios de forma recursiva
 * 
 * Funciona como mkdir -p, criando todos os diretórios intermédios
 * necessários no caminho especificado
 * 
 * @param path Caminho do diretório a criar
 * @return 0 em sucesso
 */
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

/**
 * @brief Inicializa o sistema de logging do cliente
 * 
 * Abre ou cria o ficheiro de log, criando diretórios se necessário.
 * Escreve cabeçalho se o ficheiro for novo.
 * 
 * @param ficheiroLog Caminho completo para o ficheiro de log
 * @param idCliente ID do cliente (usado no cabeçalho)
 * @return 0 em sucesso, -1 em erro
 */
int inicializarLogCliente(const char *ficheiroLog, int idCliente) {
    id_cliente_global = idCliente;
    
    // Extrair o diretório do caminho do ficheiro
    char *caminho_copia = strdup(ficheiroLog);
    char *dir = dirname(caminho_copia);
    
    // Verificar se o diretório existe, se não criar
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
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
        case EVTC_JOGO_RECEBIDO: nome_evento = "RECEBEU JOGO"; break;
        case EVTC_SOLUCAO_ENVIADA: nome_evento = "ENVIOU SOLUÇÃO"; break;
        case EVTC_RESULTADO_RECEBIDO: nome_evento = "RESULTADO"; break;
        case EVTC_SOLUCAO_CORRETA: nome_evento = "✓ CERTO"; break;
        case EVTC_SOLUCAO_INCORRETA: nome_evento = "✗ ERRADO"; break;
        case EVTC_NOVO_JOGO_PEDIDO: nome_evento = "PEDIDO JOGO"; break;
        case EVTC_CONEXAO_FECHADA: nome_evento = "FECHADO"; break;
        case EVTC_AGUARDANDO_JOGADOR: nome_evento = "AGUARDANDO"; break;
        case EVTC_SESSAO_INICIADA: nome_evento = "SESSÃO INICIADA"; break;
        case EVTC_SESSAO_TERMINADA: nome_evento = "SESSÃO TERMINADA"; break;
        case EVTC_JOGO_NUMERO: nome_evento = "JOGO #"; break;
        case EVTC_ERRO: nome_evento = "ERRO"; break;
        default: nome_evento = "?"; break;
    }
    
    // Lock exclusivo: previne race conditions em múltiplas instâncias
    flock(fileno(ficheiro_log_cliente), LOCK_EX);
    
    fprintf(ficheiro_log_cliente, "%-19s %-12s %s\n", 
            buffer_tempo, nome_evento, descricao ? descricao : "");
    fflush(ficheiro_log_cliente);
    
    // Unlock: liberta o ficheiro para outros processos
    flock(fileno(ficheiro_log_cliente), LOCK_UN);
}

void fecharLogCliente() {
    if (ficheiro_log_cliente) {
        registarEventoCliente(EVTC_CONEXAO_FECHADA, "Cliente encerrado");
        fclose(ficheiro_log_cliente);
        ficheiro_log_cliente = NULL;
    }
}

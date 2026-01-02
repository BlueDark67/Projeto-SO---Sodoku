#include "solver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "logs_cliente.h"
#include "protocolo.h"
#include "util.h"

// Variáveis globais para controlo do paralelismo
static int solucao_encontrada = 0;
static int tabuleiro_solucao[9][9];
static int last_num_threads = 0; // Nova variável global
static pthread_mutex_t solucao_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER; // NOVO

// Helper para logging seguro em threads
static void log_thread_safe(const char *msg) {
    pthread_mutex_lock(&log_mutex);
    registarEventoCliente(EVTC_ERRO, msg); // Usamos EVTC_ERRO apenas para aparecer no log genérico
    pthread_mutex_unlock(&log_mutex);
}

// Helper para validação remota (NOVO)
static void validar_bloco_remoto(int sockfd, int bloco_id, int tabuleiro[9][9], int thread_id, int idCliente) {
    // Cores ANSI para threads (0-5)
    const char *colors[] = {
        "\033[1;31m", // Vermelho
        "\033[1;32m", // Verde
        "\033[1;33m", // Amarelo
        "\033[1;34m", // Azul
        "\033[1;35m", // Magenta
        "\033[1;36m"  // Ciano
    };
    const char *reset = "\033[0m";
    const char *color = colors[thread_id % 6];

    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "%s[Thread %d] ⏳ A tentar adquirir o Mutex do Socket...%s", color, thread_id, reset);
    log_thread_safe(log_msg);

    pthread_mutex_lock(&socket_mutex);

    snprintf(log_msg, sizeof(log_msg), "%s[Thread %d] 🔒 Mutex adquirido! A enviar validação do Bloco %d...%s", color, thread_id, bloco_id, reset);
    log_thread_safe(log_msg);

    // Simular latência na secção crítica para forçar fila de espera
    usleep(100000); 

    // Preparar mensagem
    MensagemSudoku msg;
    bzero(&msg, sizeof(msg));
    msg.tipo = VALIDAR_BLOCO;
    msg.bloco_id = bloco_id;
    msg.idCliente = idCliente; // ID correto do cliente
    
    // Extrair dados do bloco
    int start_row = (bloco_id / 3) * 3;
    int start_col = (bloco_id % 3) * 3;
    int k = 0;
    for(int r = 0; r < 3; r++) {
        for(int c = 0; c < 3; c++) {
            msg.conteudo_bloco[k++] = tabuleiro[start_row + r][start_col + c];
        }
    }

    // Enviar e Receber
    if (writen(sockfd, (char *)&msg, sizeof(msg)) == sizeof(msg)) {
        MensagemSudoku resp;
        if (readn(sockfd, (char *)&resp, sizeof(resp)) == sizeof(resp)) {
            snprintf(log_msg, sizeof(log_msg), "%s[Thread %d] 📩 Resposta recebida do servidor: %s%s", color, thread_id, resp.resposta, reset);
            log_thread_safe(log_msg);
        }
    }

    pthread_mutex_unlock(&socket_mutex);
    
    snprintf(log_msg, sizeof(log_msg), "%s[Thread %d] 🔓 Mutex libertado.%s", color, thread_id, reset);
    log_thread_safe(log_msg);
}

// Protótipos internos
static int eh_valido_int(int tabuleiro[9][9], int row, int col, int num);
static int resolver_sudoku_sequencial_int(int tabuleiro[9][9], int thread_id, int *max_row_reached, int sockfd, int idCliente);

/**
 * @brief Verifica se é válido colocar um número numa posição (Versão INT)
 */
static int eh_valido_int(int tabuleiro[9][9], int row, int col, int num) {
    // Coordenadas do bloco 3x3
    int startRow = (row / 3) * 3;
    int startCol = (col / 3) * 3;

    for (int i = 0; i < 9; i++) {
        // Linha
        if (tabuleiro[row][i] == num) return 0;
        // Coluna
        if (tabuleiro[i][col] == num) return 0;
        // Bloco
        if (tabuleiro[startRow + i/3][startCol + i%3] == num) return 0;
    }
    return 1;
}

/**
 * @brief Solver Sequencial (Versão INT)
 * Usado pelas threads para resolver o resto do tabuleiro
 */
static int resolver_sudoku_sequencial_int(int tabuleiro[9][9], int thread_id, int *max_row_reached, int sockfd, int idCliente) {
    // Otimização: Verificar se outra thread já resolveu
    if (solucao_encontrada) return 0;

    int row = -1, col = -1;
    int isEmpty = 0;

    // Encontrar célula vazia
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            if (tabuleiro[i][j] == 0) {
                row = i;
                col = j;
                isEmpty = 1;
                break;
            }
        }
        if (isEmpty) break;
    }

    // Caso base: resolvido
    if (!isEmpty) {
        // Validar a última banda (blocos 7, 8, 9)
        for (int k = 0; k < 3; k++) {
             validar_bloco_remoto(sockfd, 7 + k, tabuleiro, thread_id, idCliente);
             usleep(20000);
        }
        return 1;
    }

    // LOG DE PROGRESSO (Demo Mode)
    // Se alcançámos uma nova linha mais profunda, registamos
    if (row > *max_row_reached) {
        *max_row_reached = row;
        char msg[100];
        snprintf(msg, sizeof(msg), "[Thread %d] A avançar para a linha %d...", thread_id, row);
        log_thread_safe(msg);
        
        // Validação Parcial (Demo)
        // Valida os blocos da linha anterior quando mudamos de "banda" de blocos (linhas 3 e 6)
        if (row % 3 == 0 && row > 0) {
             int banda_anterior = (row / 3) - 1;
             int bloco_inicio = banda_anterior * 3 + 1;
             
             // Validar TODOS os 3 blocos da banda anterior
             for (int k = 0; k < 3; k++) {
                 int bloco = bloco_inicio + k;
                 validar_bloco_remoto(sockfd, bloco, tabuleiro, thread_id, idCliente);
                 usleep(20000);
             }
        }
    }

    // Tentar números 1-9
    for (int num = 1; num <= 9; num++) {
        if (eh_valido_int(tabuleiro, row, col, num)) {
            tabuleiro[row][col] = num;
            
            if (resolver_sudoku_sequencial_int(tabuleiro, thread_id, max_row_reached, sockfd, idCliente)) {
                return 1;
            }
            
            tabuleiro[row][col] = 0; // Backtrack
            
            // Otimização: Se outra thread resolveu entretanto, abortar
            if (solucao_encontrada) return 0;
        }
    }
    return 0;
}

/**
 * @brief Função executada por cada thread
 */
void *thread_solver(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    char log_msg[256];
    int max_row_reached = 0; // Para controlo de logs
    
    snprintf(log_msg, sizeof(log_msg), "[Thread %d] A iniciar com numero %d na posicao (%d,%d)", 
             args->id, args->numero_arranque, args->linha_inicial, args->coluna_inicial);
    log_thread_safe(log_msg);
    
    // Simular trabalho para demo (0.1s)
    usleep(100000);

    // Colocar o número de arranque desta thread
    args->tabuleiro[args->linha_inicial][args->coluna_inicial] = args->numero_arranque;
    
    // Tentar resolver o resto
    if (resolver_sudoku_sequencial_int(args->tabuleiro, args->id, &max_row_reached, args->sockfd, args->idCliente)) {
        pthread_mutex_lock(&solucao_mutex);
        if (!solucao_encontrada) {
            solucao_encontrada = 1;
            // Copiar solução para variável global
            memcpy(tabuleiro_solucao, args->tabuleiro, sizeof(tabuleiro_solucao));
            
            snprintf(log_msg, sizeof(log_msg), "[Thread %d] ENCONTREI A SOLUÇÃO! 🏆", args->id);
            log_thread_safe(log_msg);
        }
        pthread_mutex_unlock(&solucao_mutex);
    } else {
        // Se falhou e ninguém encontrou ainda
        if (!solucao_encontrada) {
            snprintf(log_msg, sizeof(log_msg), "[Thread %d] Falhei. Caminho sem saída.", args->id);
            log_thread_safe(log_msg);
        } else {
            snprintf(log_msg, sizeof(log_msg), "[Thread %d] Abortar. Outra thread já resolveu.", args->id);
            log_thread_safe(log_msg);
        }
    }
    
    free(args); // Libertar memória dos argumentos
    return NULL;
}

/**
 * @brief Solver Paralelo
 */
int resolver_sudoku_paralelo(int tabuleiro_inicial[9][9], int sockfd, int idCliente) {
    // Reset das variáveis globais
    solucao_encontrada = 0;
    
    int row = -1, col = -1;
    int isEmpty = 0;

    // 1. Encontrar a primeira célula vazia para ramificar
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            if (tabuleiro_inicial[i][j] == 0) {
                row = i;
                col = j;
                isEmpty = 1;
                break;
            }
        }
        if (isEmpty) break;
    }

    // Se já estiver resolvido (sem zeros)
    if (!isEmpty) return 1;

    // 2. Identificar candidatos e lançar threads
    pthread_t threads[9];
    int num_threads = 0;

    for (int num = 1; num <= 9; num++) {
        if (eh_valido_int(tabuleiro_inicial, row, col, num)) {
            // Preparar argumentos
            ThreadArgs *args = malloc(sizeof(ThreadArgs));
            args->id = num_threads;
            memcpy(args->tabuleiro, tabuleiro_inicial, sizeof(args->tabuleiro));
            args->linha_inicial = row;
            args->coluna_inicial = col;
            args->numero_arranque = num;
            args->sockfd = sockfd; // Passar socket
            args->idCliente = idCliente; // Passar ID
            
            // Criar thread
            if (pthread_create(&threads[num_threads], NULL, thread_solver, args) == 0) {
                num_threads++;
            } else {
                free(args);
            }
        }
    }
    
    last_num_threads = num_threads; // Guardar contagem

    // 3. Esperar pelas threads
    printf("[PARALELO] %d threads lançadas para a primeira célula vazia (%d, %d).\n", num_threads, row, col);
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // 4. Verificar se alguma encontrou a solução
    if (solucao_encontrada) {
        memcpy(tabuleiro_inicial, tabuleiro_solucao, sizeof(tabuleiro_solucao));
        return 1;
    }

    return 0;
}

int get_num_threads_last_run() {
    return last_num_threads;
}

/**
 * @brief Wrapper principal (Mantém compatibilidade com código existente)
 */
int resolver_sudoku(char *tabuleiro, int sockfd, int idCliente) {
    int tabuleiro_int[9][9];
    
    // Converter char* para int[][]
    for (int i = 0; i < 81; i++) {
        tabuleiro_int[i/9][i%9] = tabuleiro[i] - '0';
    }
    
    printf("[DEBUG] A iniciar Solver Paralelo com Validação Remota...\n");
    
    // CHAMADA AO SOLVER PARALELO (Hardcoded como pedido)
    int result = resolver_sudoku_paralelo(tabuleiro_int, sockfd, idCliente);
    
    // Se resolveu, converter de volta
    if (result) {
        for (int i = 0; i < 81; i++) {
            tabuleiro[i] = tabuleiro_int[i/9][i%9] + '0';
        }
    }
    
    return result;
}

// --- Código antigo (mantido mas não usado diretamente pelo wrapper agora) ---
// (Pode ser removido ou mantido como fallback se quisermos implementar a escolha depois)

/**
 * @brief Verifica se é válido colocar um número numa posição específica (Versão Char)
 */
static int eh_valido(const char *tabuleiro, int pos, char num) {
    int row = pos / 9;
    int col = pos % 9;
    
    // Coordenadas do canto superior esquerdo do bloco 3x3
    int startRow = (row / 3) * 3;
    int startCol = (col / 3) * 3;

    for (int i = 0; i < 9; i++) {
        // 1. Verifica linha (mantém row fixa, varia coluna)
        if (tabuleiro[row * 9 + i] == num) return 0;
        
        // 2. Verifica coluna (mantém col fixa, varia linha)
        if (tabuleiro[i * 9 + col] == num) return 0;
        
        // 3. Verifica bloco 3x3
        // i vai de 0 a 8. 
        // i/3 dá o offset da linha (0,0,0, 1,1,1, 2,2,2)
        // i%3 dá o offset da coluna (0,1,2, 0,1,2, 0,1,2)
        int r = startRow + (i / 3);
        int c = startCol + (i % 3);
        if (tabuleiro[r * 9 + c] == num) return 0;
    }
    
    return 1;
}

/**
 * @brief Implementação do algoritmo de Backtracking (Versão Antiga/Sequencial Char)
 * 
 * @param tabuleiro Tabuleiro a resolver (modificado in-place)
 * @return 1 se resolvido, 0 se impossível
 */
int resolver_sudoku_sequencial_char(char *tabuleiro) {
    int pos = -1;
    
    // 1. Encontrar a primeira célula vazia ('0')
    for (int i = 0; i < 81; i++) {
        if (tabuleiro[i] == '0') {
            pos = i;
            break;
        }
    }

    // Caso base: Se não há células vazias, o tabuleiro está resolvido!
    if (pos == -1) return 1;

    // 2. Tentar números de '1' a '9'
    for (char num = '1'; num <= '9'; num++) {
        if (eh_valido(tabuleiro, pos, num)) {
            // Se for válido, coloca o número
            tabuleiro[pos] = num;
            
            // Chamada recursiva para o próximo passo
            if (resolver_sudoku_sequencial_char(tabuleiro)) {
                return 1; // Encontrou solução completa
            }
            
            // Se a recursão falhou, desfaz a mudança (Backtrack)
            tabuleiro[pos] = '0';
        }
    }

    // Se nenhum número funcionou nesta posição, este caminho é inválido
    return 0;
}

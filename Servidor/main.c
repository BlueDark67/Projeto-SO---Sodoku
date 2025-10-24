#include <stdio.h>
#include <string.h>
#include "config_servidor.h"
#include "jogos.h"
#include "logs.h"

void testarVerificacao(Jogo *jogo);
void menuTestes();

int main() {
    ConfigServidor config;
    Jogo jogos[MAX_JOGOS];
    int numJogos = 0;

    printf("===========================================\n");
    printf("   SERVIDOR SUDOKU \n");
    printf("===========================================\n\n");

    // 1. Ler configuração
    printf("1. A ler configuração...\n");
    if (lerConfigServidor("server.conf", &config) == 0) {
        printf("   ✓ Ficheiro de jogos: %s\n", config.ficheiroJogos);
        printf("   ✓ Ficheiro de soluções: %s\n", config.ficheiroSolucoes);
        printf("   ✓ Ficheiro de logs: %s\n\n", config.ficheiroLog);
    } else {
        printf("   ✗ Falha a ler o ficheiro de configuração!\n");
        return -1;
    }

    // 2. Inicializar sistema de logs
    printf("2. A inicializar sistema de logs...\n");
    if (inicializarLog(config.ficheiroLog) == 0) {
        printf("   ✓ Sistema de logs inicializado\n\n");
        registarEvento(0, EVT_SERVIDOR_INICIADO, "Servidor iniciado com sucesso");
    } else {
        printf("   ✗ Erro ao inicializar logs\n\n");
    }

    // 3. Carregar jogos
    printf("3. A carregar jogos...\n");
    numJogos = carregarJogos(config.ficheiroJogos, jogos, MAX_JOGOS);
    if (numJogos > 0) {
        printf("   ✓ %d jogos carregados com sucesso\n\n", numJogos);
        registarEvento(0, EVT_JOGOS_CARREGADOS, "Jogos carregados do ficheiro");
    } else {
        printf("   ✗ Erro ao carregar jogos\n\n");
        fecharLog();
        return -1;
    }

    // 4. Testar funcionalidades
    printf("4. A testar funcionalidades...\n\n");
    
    // Teste 1: Mostrar primeiro jogo
    printf("   Teste 1 - Exibir primeiro jogo (ID: %d):\n", jogos[0].idjogo);
    printf("   Tabuleiro inicial:\n");
    imprimirTabuleiro(jogos[0].tabuleiro);
    printf("\n");

    // Teste 2: Verificar solução correta
    printf("   Teste 2 - Verificar solução correta:\n");
    testarVerificacao(&jogos[0]);
    
    // Teste 3: Verificar solução errada
    printf("\n   Teste 3 - Verificar solução errada (modificada):\n");
    char solucaoErrada[82];
    strcpy(solucaoErrada, jogos[0].solucao);
    solucaoErrada[0] = '9'; // Modificar primeiro número
    
    ResultadoVerificacao resultado = verificarSolucao(solucaoErrada, jogos[0].solucao);
    printf("   Resultado: %s\n", resultado.correto ? "CORRETO" : "ERRADO");
    printf("   Números certos: %d\n", resultado.numerosCertos);
    printf("   Números errados: %d\n", resultado.numerosErrados);
    registarEvento(1, EVT_SOLUCAO_ERRADA, "Solução com erro testada");

    // Teste 4: Validar tabuleiro
    printf("\n   Teste 4 - Validar tabuleiro:\n");
    int valido = validarTabuleiro(jogos[0].tabuleiro);
    printf("   Tabuleiro inicial é %s\n", valido ? "VÁLIDO" : "INVÁLIDO");

    // Menu interativo
    printf("\n===========================================\n");
    menuTestes();

    // 5. Fechar log
    printf("\n5. A encerrar servidor...\n");
    fecharLog();
    printf("   ✓ Servidor encerrado\n");

    return 0;
}

void testarVerificacao(Jogo *jogo) {
    ResultadoVerificacao resultado = verificarSolucao(jogo->solucao, jogo->solucao);
    printf("   Resultado: %s\n", resultado.correto ? "CORRETO" : "ERRADO");
    printf("   Números certos: %d\n", resultado.numerosCertos);
    printf("   Números errados: %d\n", resultado.numerosErrados);
    registarEvento(1, EVT_SOLUCAO_CORRETA, "Solução correta testada");
}

void menuTestes() {
    printf("\nTestes da Fase 1 concluídos!\n");
    printf("\nFuncionalidades implementadas:\n");
    printf("  ✓ Leitura de ficheiro de configuração\n");
    printf("  ✓ Carregamento de jogos do ficheiro\n");
    printf("  ✓ Sistema de logs funcionando\n");
    printf("  ✓ Verificação de soluções (correta/errada)\n");
    printf("  ✓ Validação de tabuleiros\n");
    printf("  ✓ Impressão visual de tabuleiros\n");
    printf("  ✓ Estruturas de dados implementadas\n");
}
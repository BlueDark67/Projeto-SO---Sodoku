# 🎮 Guia de Testes: Sistema de Competição Fair-Play

## 📋 Resumo das Implementações

### ✅ O que foi implementado:

#### 1. **Lock Atómico com Double-Check Pattern**
- **Ficheiro**: `servidor/src/util-stream-server.c`
- **Objetivo**: Eliminar race condition na detecção do vencedor
- **Implementação**:
  ```c
  sem_wait(&dados->mutex);
  if (!dados->jogoTerminado) {
      dados->jogoTerminado = 1;
      dados->idVencedor = msg_recebida.idCliente;
      dados->tempoVitoria = time(NULL);
  }
  sem_post(&dados->mutex);
  ```
- **Resultado**: Apenas 1 cliente será marcado como vencedor, mesmo que múltiplos resolvam simultaneamente

#### 2. **Threads Configuráveis**
- **Ficheiros**: 
  - `cliente/include/config_cliente.h` (estrutura ConfigCliente)
  - `cliente/src/config_cliente.c` (leitura do parâmetro NUM_THREADS)
  - `cliente/src/solver.c` (aplicação do limite)
- **Parâmetro**: `NUM_THREADS: <1-9>` nos ficheiros `.conf`
- **Resultado**: Diferentes clientes podem usar estratégias diferentes (ex: 3 threads vs 9 threads)

#### 3. **PID-Based Shuffle**
- **Ficheiro**: `cliente/src/solver.c` (função `resolver_sudoku_paralelo`)
- **Implementação**:
  ```c
  pid_t pid = getpid();
  srand(pid);  // Seed baseada no PID
  // Fisher-Yates shuffle dos candidatos
  ```
- **Resultado**: Diferentes clientes exploram o espaço de busca em ordens diferentes

---

## 🚀 Como Testar

### Passo 1: Iniciar o Servidor
```bash
cd build
./servidor ../config/servidor/server.conf
```

### Passo 2: Iniciar Cliente A (3 threads - Conservador)
**Terminal 2:**
```bash
cd build
./cliente ../config/cliente/cliente_A.conf
```

### Passo 3: Iniciar Cliente B (9 threads - Agressivo)
**Terminal 3:**
```bash
cd build
./cliente ../config/cliente/cliente_B.conf
```

### Passo 4: Observar a Competição
Ambos os clientes:
- Entram no lobby automaticamente
- Aguardam sincronização (2+ jogadores)
- Recebem o MESMO puzzle
- Competem para resolver primeiro

**O que esperar:**
- Apenas **1 cliente** será declarado vencedor
- O outro receberá mensagem `JOGO_TERMINADO`
- Logs mostram:
  - Ordem de busca diferente (`[SHUFFLE] PID=...`)
  - Número de threads usadas (`X/Y threads lançadas`)
  - Vencedor único (`🏆 PRIMEIRO VENCEDOR!`)

---

## 🔧 Configurações Disponíveis

### `config/cliente/cliente_A.conf`
```ini
# Estratégia CONSERVADORA: Menos threads, busca mais focada
NUM_THREADS: 3
```

### `config/cliente/cliente_B.conf`
```ini
# Estratégia AGRESSIVA: Máximo paralelismo
NUM_THREADS: 9
```

### `config/cliente/cliente.conf` (Default)
```ini
# Configuração padrão
NUM_THREADS: 9
```

---

## 📊 Análise de Resultados

### Verificar Logs do Servidor
```bash
cat logs/servidor/servidor.log
```

**Procurar por:**
- `[VITÓRIA] 🏆 PRIMEIRO VENCEDOR!` - Apenas 1 ocorrência
- `Solução correta - VENCEDOR` - O cliente que ganhou
- `Solução correta - mas não foi o primeiro` - Outros clientes

### Verificar Logs dos Clientes
```bash
cat logs/clientes/cliente_*.log
```

**Procurar por:**
- `EVTC_JOGO_PERDIDO` - Cliente que perdeu
- `EVTC_SOLUCAO_CORRETA` - Cliente que ganhou

---

## 🎯 Casos de Teste Recomendados

### Teste 1: Competição Desigual (3 vs 9 threads)
- **Hipótese**: Cliente B (9 threads) deve vencer mais frequentemente
- **Executar**: 10 jogos consecutivos
- **Medir**: Taxa de vitória de cada cliente

### Teste 2: Competição Igual (9 vs 9 threads)
- **Hipótese**: PID-based shuffle cria variabilidade suficiente
- **Resultado esperado**: ~50% de vitórias para cada (distribuição justa)

### Teste 3: Teste de Race Condition
- **Objetivo**: Verificar que apenas 1 vencedor é declarado
- **Como**: Iniciar 5+ clientes simultaneamente
- **Verificação**: Apenas 1 log de `PRIMEIRO VENCEDOR!`

---

## 🐛 Troubleshooting

### Problema: Ambos clientes ainda ganham
**Causa**: Servidor antigo ainda em execução
**Solução**:
```bash
pkill servidor
make clean && make all
./build/servidor config/servidor/server.conf
```

### Problema: Cliente não lê NUM_THREADS
**Verificar**:
```bash
./build/cliente config/cliente/cliente_A.conf
# Deve mostrar: "✓ Threads Paralelas: 3"
```

### Problema: Ordem de busca sempre igual
**Verificar output**:
```
[SHUFFLE] PID=12345: Ordem embaralhada: 7 2 9 1 4 ...
```
Diferentes PIDs devem produzir ordens diferentes.

---

## 📈 Melhorias Futuras (Opcional)

### 1. Critério de Vitória por Validações
Atualmente: Primeiro a enviar solução correta
Alternativa: Contar validações e premiar eficiência

### 2. Múltiplas Rodadas
Sistema de pontuação: Melhor de 5 jogos

### 3. Dashboard em Tempo Real
Interface web mostrando progresso dos clientes

---

## ✅ Checklist de Validação

- [ ] Servidor compila sem warnings
- [ ] Cliente compila sem warnings
- [ ] Cliente A mostra "Threads Paralelas: 3"
- [ ] Cliente B mostra "Threads Paralelas: 9"
- [ ] Diferentes PIDs produzem ordens diferentes
- [ ] Apenas 1 vencedor por jogo
- [ ] Cliente perdedor recebe `JOGO_TERMINADO`
- [ ] Logs registam eventos corretamente

---

**Data de Implementação**: 2 Janeiro 2026  
**Versão**: 2.0 - Sistema de Competição Fair-Play  
**Status**: ✅ Pronto para Testes

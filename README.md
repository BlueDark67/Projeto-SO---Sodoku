# 🎮 Projeto Sudoku Cliente/Servidor

Projeto de Sistemas Operativos - Cliente/Servidor de Sudoku com concorrência e validação em tempo real.

## 📋 Estrutura do Projeto

```
ProjetoSOReorganizado/
├── Makefile            # Build system
├── README.md           # Este ficheiro
├── build/              # Executáveis compilados (cliente, servidor)
├── common/             # Código partilhado
│   ├── include/        # Headers partilhados (protocolo.h, util.h)
│   └── src/            # Implementações partilhadas (util.c)
├── servidor/
│   ├── include/        # Headers do servidor
│   ├── src/            # Código-fonte do servidor
│   └── data/           # Dados (jogos.txt)
├── cliente/
│   ├── include/        # Headers do cliente (config_cliente.h, logs_cliente.h)
│   └── src/            # Código-fonte do cliente
├── config/
│   ├── servidor/       # Configurações do servidor
│   │   └── server.conf # PORTA, MAX_FILA, MAX_JOGOS, DELAY_ERRO, MAXLINE, JOGOS, LOG
│   └── cliente/        # Configurações do cliente
│       └── cliente.conf # IP_SERVIDOR, PORTA, ID_CLIENTE, LOG
├── logs/               # Logs do sistema (gerados automaticamente)
│   ├── servidor/       # Logs do servidor
│   └── clientes/       # Logs dos clientes
└── docs/               # Documentação completa
```

## 🚀 Como Compilar

```bash
# Compilar tudo (servidor e cliente)
make all

# Limpar ficheiros compilados
make clean

# Limpar e recompilar
make clean && make all
```

## ▶️ Como Executar

**Nota:** O servidor aguarda por **2 clientes** antes de começar (barreira de sincronização).

### Terminal 1: Servidor
```bash
./build/servidor
# ou especificar ficheiro de configuração
./build/servidor config/servidor/serverPadrao.conf  # Modo produção
./build/servidor config/servidor/serverDebug.conf   # Modo desenvolvimento
```

### Terminal 2 e 3: Clientes
```bash
./build/cliente
# ou especificar ficheiro de configuração
./build/cliente config/cliente/cliente.conf
```

O servidor e cliente procuram automaticamente os ficheiros de configuração em:
- Executando da raiz: `config/servidor/serverPadrao.conf` ou `config/cliente/cliente.conf`
- Executando de `build/`: `../config/servidor/serverPadrao.conf` ou `../config/cliente/cliente.conf`

**Modos de Operação do Servidor:**
- **serverPadrao.conf**: Modo produção - logs preservados por 7 dias
- **serverDebug.conf**: Modo desenvolvimento - logs apagados ao encerrar

## ⚙️ Configuração

### Servidor (`config/servidor/serverPadrao.conf`)
```ini
# Modo de Operação
MODO: PADRAO            # PADRAO (produção) ou DEBUG (desenvolvimento)
DIAS_RETENCAO_LOGS: 7   # Dias para manter logs (modo PADRAO)

# Configuração de Rede
PORTA: 8080           # Porta TCP do servidor
MAX_FILA: 5           # Máximo de clientes em fila de espera

# Configuração de Jogos
MAX_JOGOS: 100        # Capacidade máxima de jogos a carregar
JOGOS: servidor/data/jogos.txt  # Ficheiro com jogos Sudoku

# Configuração de Sistema
DELAY_ERRO: 2         # Segundos de espera após erro (anticheat)
MAXLINE: 512          # Tamanho do buffer de comunicação
LOG: logs/servidor/server.log   # Ficheiro de log
```

### Servidor Debug (`config/servidor/serverDebug.conf`)
```ini
# Modo de Operação
MODO: DEBUG                     # Modo desenvolvimento
LIMPAR_LOGS_ENCERRAMENTO: 1     # Apaga logs ao encerrar

# (restantes configurações iguais ao modo padrão)
```

### Cliente (`config/cliente/cliente.conf`)
```ini
# Configuração de Conexão
IP_SERVIDOR: 127.0.0.1  # IP do servidor
PORTA: 8080             # Porta do servidor

# Configuração do Cliente
ID_CLIENTE: 1           # ID único do cliente
TIMEOUT_SERVIDOR: 300   # Timeout para operações (segundos)
LOG: logs/clientes/cliente_1.log  # Ficheiro de log

# Estratégia de Resolução
NUM_THREADS: 9          # Número de threads paralelas (1-9)
```

**Configurações Disponíveis:**
- `cliente.conf` - Configuração padrão (9 threads)
- `cliente_A.conf` - Estratégia conservadora (3 threads)
- `cliente_B.conf` - Estratégia agressiva (9 threads)

## 📝 Logs

Os logs são gerados automaticamente com **formatação alinhada** e **informação detalhada**:

### Servidor (`logs/servidor/server.log`)
```
IdUtilizador Hora     Acontecimento      Descrição
============ ======== ================== ===========
[Servidor]   04:23:10 Servidor Iniciado  Servidor iniciado - Porta: 8080, MaxFila: 5, MaxJogos: 100
[Servidor]   04:23:10 Jogos Carregados   100 jogos carregados de servidor/data/jogos.txt
1            04:24:12 Cliente Conectado  Novo cliente conectado de 127.0.0.1
1            04:24:12 Jogo Enviado       Cliente #1 pediu jogo - Enviado Jogo #1 (37 células preenchidas)
1            04:24:17 Solucao Errada     ✗ SOLUÇÃO INCORRETA - Cliente #1, Jogo #1: 43 erros, 38 acertos de 81 células
```

### Cliente (`logs/clientes/cliente_PID.log`)
```
Data/Hora           Evento       Descrição
------------------- ------------ ------------------------------------
2026-01-01 04:26:59 INÍCIO       Cliente #1 iniciado - Config: config/cliente/cliente.conf
2026-01-01 04:26:59 CONEXÃO      Conexão estabelecida com servidor 127.0.0.1:8080
2026-01-01 04:27:19 JOGO RX      Jogo #1 recebido (37 células preenchidas, 44 vazias)
2026-01-01 04:27:24 SOL TX       Solução enviada para Jogo #1 (38 células, tempo: 5s)
2026-01-01 04:27:24 ✗ ERRADO     ✗ SOLUÇÃO INCORRETA - Jogo #1 (tempo: 5s)
```

**Características:**
- ✓ Auto-criação de diretórios
- ✓ Formatação em colunas alinhadas
- ✓ Timestamps automáticos
- ✓ Eventos descritivos com IDs, estatísticas e símbolos
- ✓ Path resolution automático

## 🛠️ Desenvolvimento

### Comandos Úteis
```bash
# Limpar ficheiros compilados e objetos
make clean

# Ver ficheiros que serão compilados
ls servidor/src/*.c cliente/src/*.c common/src/*.c
```

### Estrutura de Código
- **Servidor**: Aceita conexões, gere jogos, verifica soluções
- **Cliente**: Conecta ao servidor, simula resolução, envia soluções
- **Common**: Protocolo de comunicação (readn/writen) e estruturas partilhadas
- **Logs**: Sistema completo de logging para servidor e cliente

### Código Documentado
Todo o código inclui **comentários explicativos** sobre:
- Propósito de cada módulo
- Fluxo de comunicação
- Sincronização e concorrência
- Path resolution
- Sistema de logging

## 🏆 Modo Competição

O sistema suporta competição justa entre múltiplos clientes:

### Testar Competição
```bash
# Terminal 1: Servidor
./build/servidor config/servidor/server.conf

# Terminal 2: Cliente A (3 threads - conservador)
./build/cliente config/cliente/cliente_A.conf

# Terminal 3: Cliente B (9 threads - agressivo)
./build/cliente config/cliente/cliente_B.conf
```

**Características:**
- ✅ Apenas 1 vencedor por jogo
- ✅ Ordem de busca diferente por cliente (PID-based shuffle)
- ✅ Estratégias diferentes (3 vs 9 threads)
- ✅ Notificação automática de derrota
- ✅ Logs detalhados de competição

## 📚 Documentação

Ver [docs/GUIA_COMPLETO_PROJETO.md](docs/GUIA_COMPLETO_PROJETO.md) para documentação completa.

## 🧪 Estado Atual

### ✅ Funcionalidades Core
- ✅ Comunicação Cliente/Servidor (TCP/IP com sockets)
- ✅ Sistema de Configuração (.conf com validação)
- ✅ Sistema de Logs (detalhado e formatado)
- ✅ Sincronização entre clientes (lobby dinâmico 2-10 jogadores)
- ✅ Verificação de soluções Sudoku
- ✅ Path resolution automático
- ✅ Código totalmente documentado

### 🎮 Sistema de Competição Fair-Play
- ✅ **Lock Atómico com Double-Check Pattern**
  - Garantia de vencedor único mesmo com resoluções simultâneas
  - Verificação atómica usando semáforos
- ✅ **Threads Configuráveis (1-9)**
  - Clientes podem usar estratégias diferentes
  - Configurável via parâmetro NUM_THREADS
- ✅ **PID-Based Shuffle**
  - Ordem de busca única por cliente
  - Variabilidade garantida entre diferentes processos
- ✅ **Solver Paralelo com Backtracking**
  - Algoritmo real de resolução de Sudoku
  - Até 9 threads explorando espaço de busca
  - Validação remota de blocos 3×3

### 📊 Sistema de Broadcast
- ✅ Notificação de fim de jogo
- ✅ Mensagem JOGO_TERMINADO para clientes perdedores
- ✅ Logs detalhados de vitória/derrota

## 👥 Autores

Projeto de Sistemas Operativos - Universidade

Guilherme Pedro - nº2124623
Tiago Alves - nº2144323
Omar Jesus nº2099223
---

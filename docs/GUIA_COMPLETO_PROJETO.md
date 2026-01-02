# 🎓 GUIA COMPLETO DO PROJETO SUDOKU CLIENTE/SERVIDOR

**Disciplina:** Sistemas Operativos  
**Projeto:** Cliente/Servidor de Sudoku em C  


---

## 📁 1. ESTRUTURA GERAL DO PROJETO

O teu colega organizou o projeto em 3 partes principais:

```
Projeto-SO---Sodoku/
│
├── 📄 Ficheiros Partilhados (usados por ambos)
│   ├── protocolo.h          → Define as mensagens trocadas
│   ├── util.h / util.c      → Funções de rede (readn, writen)
│   ├── cliente.conf         → Configuração do cliente
│   └── server.conf          → Configuração do servidor
│
├── 📂 Servidor/
│   ├── main.c               → Programa principal do servidor
│   ├── util-stream-server.c → Lógica de processamento de clientes
│   ├── jogos.c/h            → Gestão de jogos Sudoku
│   ├── logs.c/h             → Sistema de logging
│   └── config_servidor.c/h  → Leitura de configuração
│
└── 📂 Cliente/
    ├── main_cliente.c       → Programa principal do cliente
    ├── util-stream-cliente.c → Lógica de comunicação e UI
    └── config_cliente.c/h   → Leitura de configuração
```

---

## 🔧 2. COMO FUNCIONA O SISTEMA (Visão Geral)

### Fluxo Completo:

```
1. SERVIDOR ARRANCA
   ↓
   - Lê server.conf
   - Carrega jogos de jogos.txt
   - Abre porta 8080 (TCP)
   - Fica à espera de clientes
   
2. CLIENTE CONECTA
   ↓
   - Lê cliente.conf
   - Conecta ao IP do servidor
   - Pede um jogo
   
3. SERVIDOR RESPONDE
   ↓
   - Envia tabuleiro não resolvido
   - Aguarda solução
   
4. CLIENTE RESOLVE (simulação)
   ↓
   - "Resolve" o jogo
   - Envia solução ao servidor
   
5. SERVIDOR VALIDA
   ↓
   - Compara com solução correta
   - Responde "Certo" ou "Errado"
   - Regista no log
```

---

## 🖥️ 3. SERVIDOR - IMPLEMENTAÇÃO DETALHADA

### 3.1 Ficheiro Principal: `Servidor/main.c`

**O que faz:**

```c
// FASE 1: INICIALIZAÇÃO
1. Lê server.conf → sabe onde estão os jogos e logs
2. Inicializa sistema de logs
3. Carrega jogos do ficheiro jogos.txt

// FASE 2: REDE
4. Cria socket TCP (porta 8080)
5. Faz bind() à porta
6. Fica em listen() à espera de clientes

// FASE 3: SINCRONIZAÇÃO (IMPORTANTE!)
7. Cria memória partilhada com mmap()
8. Inicializa 2 semáforos:
   - mutex → protege contador de clientes
   - barreira → aguarda 2 clientes conectarem
```

**Detalhe Técnico Importante:**

```c
// O servidor usa fork() - cada cliente tem um processo-filho
for (;;) {
    newsockfd = accept(sockfd, ...);  // Aceita novo cliente
    
    if ((childpid = fork()) == 0) {
        // PROCESSO FILHO - lida com 1 cliente
        close(sockfd);  // Não precisa do socket pai
        str_echo(newsockfd, jogos, numJogos, dados);
        exit(0);
    }
    
    // PROCESSO PAI - continua a aceitar clientes
    close(newsockfd);  // Não precisa do socket do filho
}
```

**Sincronização com Barreira:**

- O 1º cliente a conectar fica **bloqueado** no `sem_wait(&dados->barreira)`
- O 2º cliente faz `sem_post(&dados->barreira)` e **desbloqueia** o primeiro
- Ambos começam a jogar simultaneamente

---

### 3.2 Lógica de Processamento: `Servidor/util-stream-server.c`

**Função principal: `str_echo()`**

```c
void str_echo(int sockfd, Jogo jogos[], int numJogos, DadosPartilhados *dados)
```

**O que faz:**

1. **Incrementa contador** de clientes (com mutex)
2. **Aguarda na barreira** (se for o 1º cliente)
3. Entra em **loop infinito** a processar mensagens:

```c
switch (msg_recebida.tipo) {
    
    case PEDIR_JOGO:
        // Cliente pediu um jogo
        → Envia sempre o jogo 0 (jogos[0])
        → Regista no log
        → Responde com ENVIAR_JOGO
        break;
    
    case ENVIAR_SOLUCAO:
        // Cliente enviou solução
        → Procura o jogo na lista
        → Chama verificarSolucao()
        → Responde "Certo" ou "Errado"
        → Regista resultado no log
        break;
}
```

---

### 3.3 Sistema de Jogos: `Servidor/jogos.c`

**Função 1: `carregarJogos()`**

```c
// Lê ficheiro CSV no formato:
// id,tabuleiro(81 chars),solução(81 chars)
// Exemplo:
// 1,000000000123456789...,123456789987654321...
```

**Função 2: `verificarSolucao()`**

```c
// Compara char-a-char:
ResultadoVerificacao res;
res.correto = (solucao == solucaoCorreta);
res.numerosCertos = count(matches);
res.numerosErrados = 81 - numerosCertos;
```

**Função 3: `validarTabuleiro()`**

```c
// Verifica regras Sudoku:
- Cada linha: sem repetições
- Cada coluna: sem repetições  
- Cada região 3x3: sem repetições
```

---

### 3.4 Sistema de Logs: `Servidor/logs.c`

**Tipos de Eventos Registados:**

```c
EVT_SERVIDOR_INICIADO = 1     → Servidor arrancou
EVT_JOGOS_CARREGADOS = 2      → Jogos carregados
EVT_CLIENTE_CONECTADO = 3     → Cliente ligou-se
EVT_CLIENTE_DESCONECTADO = 4  → Cliente desligou-se
EVT_JOGO_PEDIDO = 5           → Cliente pediu jogo
EVT_JOGO_ENVIADO = 6          → Jogo enviado
EVT_SOLUCAO_RECEBIDA = 7      → Solução recebida
EVT_SOLUCAO_VERIFICADA = 8    → Solução verificada
EVT_SOLUCAO_CORRETA = 9       → ✅ Solução estava certa
EVT_SOLUCAO_ERRADA = 10       → ❌ Solução estava errada
EVT_ERRO_GERAL = 99           → Erro qualquer
```

**Formato do Log:**

```
IdUtilizador    Hora        Acontecimento    Descricao
============    ========    =============    =========
0               14:32:15    1                "Servidor a arrancar..."
1               14:32:20    5                "Cliente pediu jogo"
1               14:32:25    9                "Solução correta"
```

---

### 3.5 Configuração: `Servidor/config_servidor.c`

**Lê o ficheiro `server.conf`:**

```
JOGOS: jogos.txt
SOLUCOES: solucoes.txt
LOG: server.log_
```

**Guarda em estrutura:**

```c
typedef struct {
    char ficheiroJogos[100];    → "jogos.txt"
    char ficheiroSolucoes[100]; → "solucoes.txt"
    char ficheiroLog[100];      → "server.log_"
} ConfigServidor;
```

---

## 💻 4. CLIENTE - IMPLEMENTAÇÃO DETALHADA

### 4.1 Ficheiro Principal: `Cliente/main_cliente.c`

**O que faz:**

```c
// FASE 1: CONFIGURAÇÃO
1. Lê cliente.conf
   → Descobre IP do servidor
   → Descobre o seu ID

// FASE 2: CONEXÃO
2. Cria socket TCP (AF_INET)
3. Converte IP com inet_pton()
4. Faz connect() ao servidor (IP:8080)

// FASE 3: JOGO
5. Chama str_cli() → função principal
6. Fecha socket e termina
```

**Código Importante:**

```c
// Converte IP de texto para formato de rede
serv_addr.sin_family = AF_INET;
serv_addr.sin_port = htons(8080);
inet_pton(AF_INET, config.ipServidor, &serv_addr.sin_addr);

// Conecta ao servidor
connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

// Passa controlo para str_cli
str_cli(stdin, sockfd, config.idCliente);
```

---

### 4.2 Lógica Principal: `Cliente/util-stream-cliente.c`

**Função principal: `str_cli()`**

```c
void str_cli(FILE *fp, int sockfd, int idCliente)
```

**Fluxo Completo (5 Passos):**

#### **PASSO 1: Pedir Jogo**

```c
MensagemSudoku msg_enviar;
msg_enviar.tipo = PEDIR_JOGO;
msg_enviar.idCliente = idCliente;

writen(sockfd, &msg_enviar, sizeof(MensagemSudoku));
```

#### **PASSO 2: Receber Jogo**

```c
readn(sockfd, &msg_receber, sizeof(MensagemSudoku));

// Guarda cópia segura
memcpy(&msg_jogo_original, &msg_receber, sizeof(MensagemSudoku));
```

#### **PASSO 3: "Resolver" (Simulação)**

```c
// IMPORTANTE: Isto é só uma simulação!
// O teu colega não implementou algoritmo real

char minha_solucao[82];
strcpy(minha_solucao, msg_jogo_original.tabuleiro);

// Atualiza UI 5 vezes (efeito visual)
for (int i = 0; i < 5; i++) {
    atualizarUICliente(&msg_jogo_original, horaInicio);
    sleep(1);
}

// "Resolve": preenche primeira célula vazia com '9'
for (int i = 0; i < 81; i++) {
    if (minha_solucao[i] == '0') {
        minha_solucao[i] = '9';
        break;  // Para no primeiro
    }
}
```

**⚠️ NOTA CRÍTICA:** O cliente **NÃO resolve** o Sudoku de verdade! Apenas muda um '0' para '9'. É aqui que tu terás de implementar o algoritmo de backtracking com threads.

#### **PASSO 4: Enviar Solução**

```c
msg_enviar.tipo = ENVIAR_SOLUCAO;
msg_enviar.idJogo = msg_jogo_original.idJogo;
strcpy(msg_enviar.tabuleiro, minha_solucao);

writen(sockfd, &msg_enviar, sizeof(MensagemSudoku));
```

#### **PASSO 5: Receber Resultado**

```c
readn(sockfd, &msg_receber, sizeof(MensagemSudoku));

// msg_receber.resposta contém "Certo" ou "Errado"
printf("Resultado: %s\n", msg_receber.resposta);
```

---

### 4.3 Interface Visual: `atualizarUICliente()`

**O que faz:**

```c
void atualizarUICliente(MensagemSudoku *msg, time_t horaInicio) {
    system("clear");  // Limpa ecrã (Linux/macOS)
    
    // Cabeçalho com info
    printf("ID Jogo: %d\n", msg->idJogo);
    printf("Tempo: %.0f segundos\n", difftime(time(NULL), horaInicio));
    
    // Imprime tabuleiro formatado
    imprimirTabuleiroCliente(msg->tabuleiro);
}
```

**Função `imprimirTabuleiroCliente()`:**

```c
// Transforma string de 81 chars em grelha visual:
// "000123456..." vira:

 .  .  . | 1  2  3 | 4  5  6
 .  .  . | .  .  . | .  .  .
 .  .  . | .  .  . | .  .  .
---------+---------+---------
 .  .  . | .  .  . | .  .  .
 ...
```

---

### 4.4 Configuração: `Cliente/config_cliente.c`

**Lê o ficheiro `cliente.conf`:**

```
IP_SERVIDOR: 127.0.0.1
ID_CLIENTE: 1
LOG: cliente.log
```

**Parsing com função `trim()`:**

```c
// Remove espaços antes e depois do valor
// "127.0.0.1   " → "127.0.0.1"
```

**Guarda em estrutura:**

```c
typedef struct {
    char ipServidor[50];    → "127.0.0.1"
    int idCliente;          → 1
    char ficheiroLog[100];  → "cliente.log" (não usado ainda)
} ConfigCliente;
```

---

## 📡 5. PROTOCOLO DE COMUNICAÇÃO

### 5.1 Ficheiro: `protocolo.h`

**Tipos de Mensagens:**

```c
typedef enum {
    PEDIR_JOGO = 1,        // Cliente → Servidor
    ENVIAR_JOGO = 2,       // Servidor → Cliente
    ENVIAR_SOLUCAO = 3,    // Cliente → Servidor
    RESPOSTA_SOLUCAO = 4   // Servidor → Cliente
} TipoMensagem;
```

**Estrutura da Mensagem:**

```c
typedef struct {
    TipoMensagem tipo;   // Que tipo de mensagem é?
    int idCliente;       // Quem está a enviar?
    int idJogo;          // Sobre que jogo?
    char tabuleiro[82];  // 81 células + '\0'
    char resposta[50];   // "Certo", "Errado", etc.
} MensagemSudoku;
```

**Tamanho:** `sizeof(MensagemSudoku)` = ~150 bytes (enviado como bloco binário)

---

### 5.2 Diagrama de Comunicação

```
CLIENTE                           SERVIDOR
   |                                 |
   |  (1) PEDIR_JOGO                 |
   |  idCliente=1                    |
   |-------------------------------->|
   |                                 |
   |                                 | (Procura jogo na lista)
   |                                 | (Regista no log)
   |                                 |
   |  (2) ENVIAR_JOGO                |
   |  idJogo=0                       |
   |  tabuleiro="000123..."          |
   |<--------------------------------|
   |                                 |
   | (Cliente "resolve")             |
   |                                 |
   |  (3) ENVIAR_SOLUCAO             |
   |  idJogo=0                       |
   |  tabuleiro="999123..."          |
   |-------------------------------->|
   |                                 |
   |                                 | (Compara com solução correta)
   |                                 | (Regista resultado no log)
   |                                 |
   |  (4) RESPOSTA_SOLUCAO           |
   |  resposta="Errado"              |
   |<--------------------------------|
   |                                 |
   | (Mostra resultado ao jogador)   |
   |                                 |
```

---

### 5.3 Garantia de Integridade: `util.c`

**Problema:** `read()` e `write()` podem enviar/receber menos bytes que o pedido.

**Solução:** Funções que garantem transferência completa:

```c
int readn(int fd, char *ptr, int nbytes) {
    int nleft = nbytes;
    while (nleft > 0) {
        nread = read(fd, ptr, nleft);
        if (nread == 0) break;  // EOF
        nleft -= nread;
        ptr += nread;  // Avança ponteiro
    }
    return (nbytes - nleft);  // Quantos leu
}
```

```c
int writen(int fd, char *ptr, int nbytes) {
    int nleft = nbytes;
    while (nleft > 0) {
        nwritten = write(fd, ptr, nleft);
        nleft -= nwritten;
        ptr += nwritten;  // Avança ponteiro
    }
    return nbytes;
}
```

**Uso:**

```c
// Envia a estrutura inteira de uma vez
writen(sockfd, (char*)&msg, sizeof(MensagemSudoku));

// Recebe a estrutura inteira de uma vez
readn(sockfd, (char*)&msg, sizeof(MensagemSudoku));
```

---

## 🧪 6. COMO TESTAR O PROJETO

### 6.1 Compilar o Código

**Makefile**

```makefile
# Compilador e flags
CC = gcc
CFLAGS = -Wall -Wextra -g -I.

# Alvos principais
TARGET_SERVER = servidorSudoku
TARGET_CLIENT = clienteSudoku

# Compilar ambos
all: $(TARGET_SERVER) $(TARGET_CLIENT)

# Servidor
$(TARGET_SERVER): $(SERVER_OBJS) $(UTIL_O)
	$(CC) $(CFLAGS) -o $(TARGET_SERVER) $(SERVER_OBJS) $(UTIL_O) -lpthread

# Cliente
$(TARGET_CLIENT): $(CLIENT_OBJS) $(UTIL_O)
	$(CC) $(CFLAGS) -o $(TARGET_CLIENT) $(CLIENT_OBJS) $(UTIL_O)
```

**Passos de Compilação:**

```bash
# Na pasta raiz do projeto
make clean  # Limpa ficheiros antigos
make        # Compila servidor e cliente

# Resultado:
# → servidorSudoku (executável do servidor)
# → clienteSudoku (executável do cliente)
```

---

### 6.2 Preparar Ficheiros de Configuração

**1. Verifica `server.conf`:**

```bash
cat server.conf
```

Deve conter:

```
JOGOS: jogos.txt
SOLUCOES: solucoes.txt
LOG: server.log_
```

**2. Verifica `cliente.conf` (ou `Cliente/cliente.config`):**

```bash
cat cliente.conf
```

Deve conter:

```
IP_SERVIDOR: 127.0.0.1
ID_CLIENTE: 1
LOG: cliente.log
```

**3. Verifica se `jogos.txt` existe e tem jogos:**

```bash
head -n 3 jogos.txt
```

Formato esperado (CSV):

```
1,000000000123456789...,987654321123456789...
2,001002003000000000...,123456789987654321...
```

---

### 6.3 Executar o Sistema

#### **Terminal 1: Servidor**

```bash
./servidorSudoku
```

Saída esperada:

```
===========================================
   SERVIDOR SUDOKU
===========================================

1. A ler configuração...
2. A inicializar logs em server.log_...
3. A carregar jogos de jogos.txt...
   ✓ 2 jogos carregados.

4. A criar socket TCP (AF_INET)...
5. A fazer bind à porta 8080...
6. A escutar na porta 8080...
   ✓ Servidor pronto. À espera de clientes.
```

#### **Terminal 2: Cliente 1**

```bash
./clienteSudoku
```

Saída esperada:

```
===========================================
CLIENTE SUDOKU
===========================================

1. A ler configuração (cliente.conf)...
   ✓ A ligar ao IP: 127.0.0.1
   ✓ O meu ID é: 1

2. A criar socket TCP (AF_INET)...
3. A ligar ao servidor 127.0.0.1:8080...
   ✓ Ligado com sucesso!

Cliente: A pedir jogo ao servidor...

[Aguarda Cliente 2...]
```

**⚠️ NOTA:** O Cliente 1 **fica bloqueado** na barreira, aguardando o Cliente 2!

#### **Terminal 3: Cliente 2**

```bash
# Edita cliente.conf primeiro:
# ID_CLIENTE: 2

./clienteSudoku
```

Quando o Cliente 2 conecta:

- Cliente 1 é desbloqueado
- Ambos recebem o jogo
- Ambos "resolvem" (simulação)
- Ambos enviam solução
- Ambos recebem "Errado" (porque a simulação não resolve corretamente)

---

### 6.4 Verificar Logs

**No servidor:**

```bash
cat server.log_
```

Exemplo:

```
IdUtilizador    Hora        Acontecimento    Descricao
============    ========    =============    =========
0               15:30:12    1                "Servidor a arrancar..."
0               15:30:12    2                "Jogos carregados"
1               15:30:15    3                "192.168.1.100"
1               15:30:15    5                "Cliente pediu jogo"
1               15:30:15    6                "Jogo 0 enviado"
2               15:30:20    3                "192.168.1.101"
1               15:30:25    7                "Solução recebida"
1               15:30:25    10               "Solução errada"
```

---

## 📊 7. RESUMO EXECUTIVO

### ✅ O QUE ESTÁ IMPLEMENTADO

| Componente | Estado | Funcionalidade |
|------------|--------|----------------|
| **Servidor** | ✅ **Completo** | Socket TCP, fork multi-processo, sincronização com semáforos |
| **Cliente** | ⚠️ **Básico** | Conexão TCP, UI visual, fluxo de comunicação |
| **Protocolo** | ✅ **Funcional** | 4 tipos de mensagens, transferência binária segura |
| **Logs** | ✅ **Completo** | 11 eventos, timestamps, registo de tudo |
| **Configuração** | ✅ **Completo** | Parsing de ficheiros .conf, estruturas definidas |
| **Validação** | ✅ **Completo** | Verifica linhas/colunas/regiões, compara soluções |
| **Solver** | ❌ **Simulação** | Apenas troca '0' por '9' (não resolve de verdade) |
| **Concorrência** | ❌ **Ausente** | Sem threads, sem paralelização |

---

### ❌ O QUE FALTA IMPLEMENTAR (Para a Tua Estratégia)

#### 1. **Algoritmo de Backtracking**

- Resolver Sudoku de verdade
- Validar regras durante resolução

#### 2. **Concorrência (9 Threads)**

- Criar threads com `pthread_create()`
- Cada thread assume número diferente (1-9)
- Sincronização com mutex

#### 3. **Validação Parcial de Blocos**

- Novo tipo de mensagem no protocolo
- Validar blocos 3×3 incrementalmente
- Backtrack imediato se bloco inválido

---

## 🎯 8. PONTOS-CHAVE PARA TI

### Conceitos Importantes que o Teu Colega Implementou:

#### 1. **Fork vs Threads:**

- Servidor usa `fork()` → cada cliente = 1 processo independente
- Tu vais usar `pthread` no cliente → 9 threads no mesmo processo

#### 2. **Memória Partilhada (mmap):**

- Servidor precisa disso porque processos-filho não partilham memória
- Threads partilham memória naturalmente (mais simples para ti!)

#### 3. **Semáforos:**

- Servidor usa para sincronizar 2 clientes
- Tu vais usar mutex para proteger variável `solucao_encontrada`

#### 4. **Comunicação Bloqueante:**

- `readn()` e `writen()` **bloqueiam** até completar
- Durante validações parciais, o solver vai "pausar" à espera do servidor

---

## 🚀 9. PRÓXIMOS PASSOS RECOMENDADOS

Para implementares a tua estratégia, sugiro esta ordem:

### **Fase 1: Algoritmo Básico** (2-3h)

1. Implementar `resolver_sudoku_backtrack()` single-threaded
2. Testar se resolve jogos simples
3. Substituir a simulação em `util-stream-cliente.c`

### **Fase 2: Concorrência** (3-4h)

1. Adicionar `-lpthread` ao Makefile do cliente
2. Criar estrutura `ThreadArgs`
3. Lançar 9 threads na função `str_cli()`
4. Implementar `resolver_thread()`

### **Fase 3: Validação Parcial** (2-3h)

1. Adicionar tipos 5 e 6 ao `protocolo.h`
2. Implementar `validarBlocoEspecifico()` no servidor
3. Integrar validações no solver do cliente

---

## 💡 DICAS FINAIS

### Para entenderes melhor o código:

```bash
# Ver fluxo de execução do servidor
grep -n "printf" Servidor/main.c

# Ver todas as mensagens registadas no log
grep -n "registarEvento" Servidor/util-stream-server.c

# Ver estrutura de dados principal
grep -n "typedef struct" protocolo.h
```

### Para depurar:

```bash
# Compilar com símbolos de debug (já está no Makefile)
make clean && make

# Executar com gdb
gdb ./clienteSudoku
(gdb) break str_cli
(gdb) run
```

---

## 🎉 CONCLUSÃO

Agora tens uma visão completa do projeto!

### Perguntas que podes fazer:

- "Mostra-me em detalhe como funciona a barreira de sincronização"
- "Como implemento o backtracking básico?"
- "Explica-me a diferença entre fork e pthread"
- "Como adiciono threads ao cliente?"

**Estou aqui para te ajudar com qualquer parte específica!** 👨‍💻

---

## 🏆 10. SISTEMA DE COMPETIÇÃO FAIR-PLAY (NOVO)

### 10.1 Problema Identificado: Race Condition

**Situação Anterior:**
- 2 clientes resolvem o mesmo puzzle simultaneamente
- Ambos enviam solução ao mesmo tempo
- Servidor marca **ambos** como vencedores
- Não há vencedor único

**Causa Raiz:**
```c
// Cliente A verifica
if (!dados->jogoTerminado) {  // false
    // Cliente B verifica ao MESMO TEMPO
    if (!dados->jogoTerminado) {  // AINDA false!
        dados->jogoTerminado = 1;  // Ambos marcam
    }
}
```

---

### 10.2 Solução Implementada: Lock Atómico

**Double-Check Pattern em `util-stream-server.c`:**

```c
if (resultado.correto) {
    int precisa_marcar = 0;
    
    sem_wait(&dados->mutex);  // 🔒 LOCK ATÓMICO
    
    // Segunda verificação (agora protegida)
    if (!dados->jogoTerminado) {
        dados->jogoTerminado = 1;
        dados->idVencedor = msg_recebida.idCliente;
        dados->tempoVitoria = time(NULL);
        precisa_marcar = 1;
        
        printf("🏆 PRIMEIRO VENCEDOR!\n");
    } else {
        printf("⏱️ Solução correta mas cliente %d ganhou primeiro\n",
               dados->idVencedor);
    }
    
    sem_post(&dados->mutex);  // 🔓 UNLOCK
}
```

**Garantia:** Apenas 1 cliente entra na seção crítica por vez.

---

### 10.3 Threads Configuráveis

**Problema:** Todos os clientes usam mesma estratégia (9 threads).

**Solução:** Parâmetro `NUM_THREADS` nos ficheiros `.conf`

#### Estrutura em `config_cliente.h`:
```c
typedef struct {
    char ipServidor[50];
    int idCliente;
    int porta;
    int timeoutServidor;
    char ficheiroLog[100];
    int numThreads;  // NOVO: 1-9 threads
} ConfigCliente;
```

#### Leitura em `config_cliente.c`:
```c
else if (strcmp(chave, "NUM_THREADS") == 0) {
    config->numThreads = atoi(valor_limpo);
    if (config->numThreads < 1) config->numThreads = 1;
    if (config->numThreads > 9) config->numThreads = 9;
}
```

#### Aplicação em `solver.c`:
```c
int resolver_sudoku_paralelo(int tab[9][9], int sockfd, 
                             int idCliente, int numThreads) {
    // Identifica candidatos válidos
    int candidatos[9];
    int num_candidatos = 0;
    
    for (int num = 1; num <= 9; num++) {
        if (eh_valido(tab, row, col, num)) {
            candidatos[num_candidatos++] = num;
        }
    }
    
    // Limita ao número configurado
    int threads_a_criar = min(num_candidatos, numThreads);
    
    // Cria apenas as threads necessárias
    for (int i = 0; i < threads_a_criar; i++) {
        pthread_create(&threads[i], NULL, thread_solver, args);
    }
}
```

**Configurações Disponíveis:**
- `cliente_A.conf`: 3 threads (conservador)
- `cliente_B.conf`: 9 threads (agressivo)
- `cliente.conf`: 9 threads (padrão)

---

### 10.4 PID-Based Shuffle

**Problema:** Clientes com mesmo número de threads exploram na mesma ordem.

**Solução:** Embaralhar candidatos usando PID como seed.

#### Implementação em `solver.c`:
```c
// Obter candidatos válidos
int candidatos[9];
int num_candidatos = 0;
for (int num = 1; num <= 9; num++) {
    if (eh_valido(tab, row, col, num)) {
        candidatos[num_candidatos++] = num;
    }
}

// SHUFFLE baseado no PID
pid_t pid = getpid();
srand(pid);  // Seed única por processo

// Fisher-Yates shuffle
for (int i = num_candidatos - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    int temp = candidatos[i];
    candidatos[i] = candidatos[j];
    candidatos[j] = temp;
}

printf("[SHUFFLE] PID=%d: Ordem embaralhada: ", pid);
for (int i = 0; i < num_candidatos; i++) {
    printf("%d ", candidatos[i]);
}
printf("\n");
```

**Exemplo de Output:**
```
[SHUFFLE] PID=12345: Ordem embaralhada: 7 2 9 1 4 6 3 5 8
[SHUFFLE] PID=12348: Ordem embaralhada: 3 8 1 9 2 5 7 4 6
```

**Resultado:** Diferentes PIDs → Diferentes ordens → Variabilidade garantida

---

### 10.5 Sistema de Broadcast

**Quando cliente perde:**

#### Servidor envia `JOGO_TERMINADO`:
```c
// Em util-stream-server.c (loop de aguardar solução)
sem_wait(&dados->mutex);
if (dados->jogoTerminado && dados->idVencedor != meu_id) {
    int vencedor = dados->idVencedor;
    sem_post(&dados->mutex);
    
    // Notificar derrota
    MensagemSudoku msg_derrota;
    msg_derrota.tipo = JOGO_TERMINADO;
    msg_derrota.idCliente = vencedor;
    snprintf(msg_derrota.resposta, sizeof(msg_derrota.resposta),
             "Cliente %d ganhou primeiro!", vencedor);
    
    writen(sockfd, &msg_derrota, sizeof(msg_derrota));
    registarEvento(meu_id, EVT_JOGO_PERDIDO, "Derrotado");
    goto cleanup_e_sair;
}
sem_post(&dados->mutex);
```

#### Cliente recebe e exibe:
```c
// Em util-stream-cliente.c
if (msg_receber.tipo == JOGO_TERMINADO) {
    printf("\n");
    printf("═══════════════════\n");
    printf("   ⚠️  JOGO TERMINADO\n");
    printf("═══════════════════\n");
    printf("Cliente %d encontrou a solução primeiro!\n", 
           msg_receber.idCliente);
    printf("Resultado: DERROTA 😞\n");
    printf("═══════════════════\n");
    
    registarEventoCliente(EVTC_JOGO_PERDIDO, "Derrotado");
    return;  // Encerra sessão
}
```

---

### 10.6 Como Testar Competição

#### Terminal 1: Servidor
```bash
./build/servidor config/servidor/server.conf
```

#### Terminal 2: Cliente A (3 threads)
```bash
./build/cliente config/cliente/cliente_A.conf
```

#### Terminal 3: Cliente B (9 threads)
```bash
./build/cliente config/cliente/cliente_B.conf
```

**O que esperar:**
1. Ambos entram no lobby
2. Servidor dispara jogo quando 2+ clientes conectados
3. Ambos recebem o MESMO puzzle
4. Ordem de busca diferente:
   ```
   [SHUFFLE] PID=12345: Ordem: 7 2 9 1 4 6 3 5 8
   [SHUFFLE] PID=12348: Ordem: 3 8 1 9 2 5 7 4 6
   ```
5. Cliente A usa 3 threads, Cliente B usa 9 threads
6. **Apenas 1 vencedor** declarado:
   ```
   [VITÓRIA] 🏆 PRIMEIRO VENCEDOR! Cliente 12348
   [INFO] ⏱️ Cliente 12345 solução correta mas não foi primeiro
   ```
7. Cliente perdedor recebe `JOGO_TERMINADO` e encerra

---

### 10.7 Análise de Logs

#### Servidor (`logs/servidor/server.log`):
```
IdUtilizador Hora     Acontecimento         Descrição
============ ======== ==================    ===========
12345        10:23:15 Solucao Correta       Solução correta - mas não foi o primeiro
12348        10:23:15 Solucao Correta       Solução correta - VENCEDOR
12345        10:23:15 Jogo Perdido          Jogo terminado - Cliente 12348 venceu
```

#### Cliente Vencedor:
```
Data/Hora           Evento       Descrição
------------------- ------------ -----------
2026-01-02 10:23:15 ✅ CORRETO   ✅ SOL. CORRETA - Jogo #1
```

#### Cliente Perdedor:
```
Data/Hora           Evento       Descrição
------------------- ------------ -----------
2026-01-02 10:23:15 DERROTA      Derrotado - Cliente 12348 ganhou
```

---

### 10.8 Resumo das Garantias

✅ **Vencedor Único:** Double-check pattern com semáforos  
✅ **Estratégias Diferentes:** NUM_THREADS configurável (1-9)  
✅ **Variabilidade:** PID-based shuffle da ordem de busca  
✅ **Fairness:** Todos recebem o mesmo puzzle simultaneamente  
✅ **Notificação:** Broadcast de JOGO_TERMINADO aos perdedores  
✅ **Logs Completos:** Detalhes de vitória/derrota registados  

---

## 🚀 11. MELHORIA CONTÍNUA

### Próximas Evoluções Possíveis:

1. **Critério de Vitória por Eficiência**
   - Contar validações remotas
   - Premiar solução com menos validações

2. **Sistema de Pontos**
   - Melhor de 5 jogos
   - Ranking de clientes

3. **Dashboard em Tempo Real**
   - Interface web com progresso
   - Visualização do espaço de busca

4. **Análise de Performance**
   - Tempo médio por thread
   - Taxa de sucesso por estratégia

---


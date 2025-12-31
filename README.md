# 🎮 Projeto Sudoku Cliente/Servidor

Projeto de Sistemas Operativos - Cliente/Servidor de Sudoku com concorrência e validação em tempo real.

## 📋 Estrutura do Projeto

```
Projeto-SO-Sudoku/
├── build/              # Executáveis compilados
├── common/             # Código partilhado
│   ├── include/        # Headers partilhados (protocolo.h, util.h)
│   └── src/            # Implementações partilhadas (util.c)
├── servidor/
│   ├── include/        # Headers do servidor
│   ├── src/            # Código-fonte do servidor
│   └── data/           # Dados (jogos.txt)
├── cliente/
│   ├── include/        # Headers do cliente
│   └── src/            # Código-fonte do cliente
├── config/             # Configurações (*.conf)
├── logs/               # Logs do sistema
│   ├── servidor/       # Logs do servidor
│   └── clientes/       # Logs dos clientes
└── docs/               # Documentação
```

## 🚀 Como Compilar

```bash
# Compilar tudo
make

# Limpar e recompilar
make rebuild

# Ver informações
make info
```

## ▶️ Como Executar

### Terminal 1: Servidor
```bash
make run-server
# ou
./build/servidorSudoku
```

### Terminal 2: Cliente
```bash
make run-client
# ou
./build/clienteSudoku
```

## ⚙️ Configuração

### Servidor (`config/server.conf`)
```
JOGOS: servidor/data/jogos.txt
SOLUCOES: servidor/data/jogos.txt
LOG: logs/servidor/server.log
```

### Cliente (`config/cliente.conf`)
```
IP_SERVIDOR: 127.0.0.1
ID_CLIENTE: 1
LOG: logs/clientes/cliente_1.log
```

## 📝 Logs

- **Servidor**: `logs/servidor/server.log`
- **Clientes**: `logs/clientes/cliente_{ID}.log`

## 🛠️ Desenvolvimento

### Limpar ficheiros compilados
```bash
make clean
```

### Estrutura de Logs
- `logs/servidor/server.log` - Todos os eventos do servidor
- `logs/clientes/cliente_N.log` - Logs individuais por cliente

## 📚 Documentação

Ver [docs/GUIA_COMPLETO_PROJETO.md](docs/GUIA_COMPLETO_PROJETO.md) para documentação completa.

## 🧪 Estado Atual

- ✅ Comunicação Cliente/Servidor (TCP/IP)
- ✅ Sistema de Logs
- ✅ Sincronização entre clientes
- ⏳ Solver com threads (em desenvolvimento)
- ⏳ Validação parcial de blocos (em desenvolvimento)

## 👥 Autores

Projeto de Sistemas Operativos - Universidade

---

**Última atualização:** 31 de Dezembro de 2025

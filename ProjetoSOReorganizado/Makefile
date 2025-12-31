# Makefile para Servidor e Cliente Sudoku - Projeto SO
# Estrutura reorganizada com common/, servidor/, cliente/

# Compilador e flags
CC = gcc
# -I para adicionar diretórios de includes
CFLAGS = -Wall -Wextra -g -Icommon/include -Iservidor/include -Icliente/include

# --- Alvos Principais ---
TARGET_SERVER = build/servidor
TARGET_CLIENT = build/cliente

# --- Diretórios ---
COMMON_SRC = common/src
COMMON_INC = common/include
SERVER_SRC = servidor/src
SERVER_INC = servidor/include
CLIENT_SRC = cliente/src
CLIENT_INC = cliente/include
BUILD_DIR = build

# --- Ficheiros Partilhados (common) ---
COMMON_SRCS = $(COMMON_SRC)/util.c
COMMON_OBJS = $(COMMON_SRCS:.c=.o)

# --- Ficheiros do SERVIDOR ---
SERVER_SRCS = $(SERVER_SRC)/main.c $(SERVER_SRC)/config_servidor.c $(SERVER_SRC)/jogos.c $(SERVER_SRC)/logs.c $(SERVER_SRC)/util-stream-server.c
SERVER_OBJS = $(SERVER_SRCS:.c=.o)

# --- Ficheiros do CLIENTE ---
CLIENT_SRCS = $(CLIENT_SRC)/main_cliente.c $(CLIENT_SRC)/config_cliente.c $(CLIENT_SRC)/util-stream-cliente.c
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)


# --- Regras Principais ---

# Cria o diretório build se não existir
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# A regra 'all' (default) compila ambos
all: $(BUILD_DIR) $(TARGET_SERVER) $(TARGET_CLIENT)

# Regra para compilar o SERVIDOR
$(TARGET_SERVER): $(SERVER_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $(TARGET_SERVER) $(SERVER_OBJS) $(COMMON_OBJS) -lpthread
	@echo "✓ Servidor compilado: $(TARGET_SERVER)"

# Regra para compilar o CLIENTE
$(TARGET_CLIENT): $(CLIENT_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $(TARGET_CLIENT) $(CLIENT_OBJS) $(COMMON_OBJS)
	@echo "✓ Cliente compilado: $(TARGET_CLIENT)"


# --- Regras de Compilação (.c para .o) ---

# Compila ficheiros em common/src/
$(COMMON_SRC)/%.o: $(COMMON_SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compila ficheiros em servidor/src/
$(SERVER_SRC)/%.o: $(SERVER_SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compila ficheiros em cliente/src/
$(CLIENT_SRC)/%.o: $(CLIENT_SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@


# --- Regras de Limpeza e Execução ---

# Regra para limpar tudo
clean:
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT) $(COMMON_OBJS) $(SERVER_OBJS) $(CLIENT_OBJS)
	rm -rf $(BUILD_DIR)
	@echo "✓ Ficheiros limpos"

# Regra para limpar e recompilar tudo
rebuild: clean all

# Regra para executar o servidor
run-server: $(TARGET_SERVER)
	@echo "--- A executar o Servidor ---"
	$(TARGET_SERVER)

# Regra para executar o cliente
run-client: $(TARGET_CLIENT)
	@echo "--- A executar o Cliente ---"
	$(TARGET_CLIENT)

# Diz ao Make que estas regras não criam ficheiros
.PHONY: all clean rebuild run-server run-client
# Makefile para Servidor e Cliente Sudoku - Fase 2

# Compilador e flags
CC = gcc
# -g para debug, -I. diz ao compilador para procurar ficheiros .h na pasta atual (root)
CFLAGS = -Wall -Wextra -g -I.

# --- Alvos Principais ---
TARGET_SERVER = servidorSudoku
TARGET_CLIENT = clienteSudoku

# --- Ficheiros Partilhados ---
# O util.o será usado por ambos
UTIL_O = util.o

# --- Ficheiros do SERVIDOR ---
SDIR = Servidor
# Lista de todos os ficheiros .c na pasta servidor/
SERVER_SRCS = $(SDIR)/main.c $(SDIR)/config_servidor.c $(SDIR)/jogos.c $(SDIR)/logs.c $(SDIR)/util-stream-server.c
# Converte a lista de .c para .o (ex: servidor/main.c -> servidor/main.o)
SERVER_OBJS = $(SERVER_SRCS:.c=.o)

# --- Ficheiros do CLIENTE ---
CDIR = Cliente
# Lista de todos os ficheiros .c na pasta cliente/
CLIENT_SRCS = $(CDIR)/main_cliente.c $(CDIR)/config_cliente.c $(CDIR)/util-stream-cliente.c
# Converte a lista de .c para .o
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)


# --- Regras Principais ---

# A regra 'all' (default) compila ambos
all: $(TARGET_SERVER) $(TARGET_CLIENT)

# Regra para compilar o SERVIDOR
$(TARGET_SERVER): $(SERVER_OBJS) $(UTIL_O)
	$(CC) $(CFLAGS) -o $(TARGET_SERVER) $(SERVER_OBJS) $(UTIL_O) -lpthread
	@echo "✓ Servidor compilado: ./$(TARGET_SERVER)"

# Regra para compilar o CLIENTE
$(TARGET_CLIENT): $(CLIENT_OBJS) $(UTIL_O)
	$(CC) $(CFLAGS) -o $(TARGET_CLIENT) $(CLIENT_OBJS) $(UTIL_O)
	@echo "✓ Cliente compilado: ./$(TARGET_CLIENT)"


# --- Regras de Compilação (.c para .o) ---

# Regra para compilar o util.c partilhado
$(UTIL_O): util.c util.h
	$(CC) $(CFLAGS) -c util.c -o $(UTIL_O)

# Regra "mágica" para compilar qualquer .c da pasta servidor/
$(SDIR)/%.o: $(SDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Regra "mágica" para compilar qualquer .c da pasta cliente/
$(CDIR)/%.o: $(CDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@


# --- Regras de Limpeza e Execução ---

# Regra para limpar tudo
clean:
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT) $(UTIL_O) $(SERVER_OBJS) $(CLIENT_OBJS)
	@echo "Ficheiros limpos"

# Regra para limpar e recompilar tudo
rebuild: clean all

# Regra para executar o servidor
run-server: $(TARGET_SERVER)
	@echo "--- A executar o Servidor ---"
	./$(TARGET_SERVER)

# Regra para executar o cliente
run-client: $(TARGET_CLIENT)
	@echo "--- A executar o Cliente ---"
	./$(TARGET_CLIENT)

# Diz ao Make que estas regras não criam ficheiros
.PHONY: all clean rebuild run-server run-client
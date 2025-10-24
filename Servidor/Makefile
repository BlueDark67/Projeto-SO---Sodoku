# Makefile para Servidor Sudoku - Fase 1

CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = servidorSudoku
OBJS = main.o config_servidor.o jogos.o logs.o

# Regra principal
all: $(TARGET)

# Compilar o executável
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)
	@echo "Compilação concluída: ./$(TARGET)"

# Compilar main.c
main.o: main.c config_servidor.h jogos.h logs.h
	$(CC) $(CFLAGS) -c main.c

# Compilar config_servidor.c
config_servidor.o: config_servidor.c config_servidor.h
	$(CC) $(CFLAGS) -c config_servidor.c

# Compilar jogos.c
jogos.o: jogos.c jogos.h config_servidor.h
	$(CC) $(CFLAGS) -c jogos.c

# Compilar logs.c
logs.o: logs.c logs.h
	$(CC) $(CFLAGS) -c logs.c

# Limpar ficheiros compilados
clean:
	rm -f $(OBJS) $(TARGET) servidor.log
	@echo "Ficheiros limpos"

# Limpar e recompilar
rebuild: clean all

# Executar o programa
run: $(TARGET)
	./$(TARGET)

# Regras que não são ficheiros
.PHONY: all clean rebuild run
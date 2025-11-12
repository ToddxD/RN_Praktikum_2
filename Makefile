# Name des zu erstellenden Programms
TARGET = server

# Compiler und Flags
CC = gcc
CFLAGS = -Wall

# Quell- und Objektdateien
SRCS = server.c sockaddr_array.c
OBJS = $(SRCS:.c=.o)

# Standard-Target: Programm bauen
all: $(TARGET)

# Link-Schritt
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Kompilierschritt für einzelne .c-Dateien
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Aufräumen
clean:
	rm -f $(OBJS) $(TARGET)

# Optional: „make run“ für direkten Start
run: $(TARGET)
	./$(TARGET)

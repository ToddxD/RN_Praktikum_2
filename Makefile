# Compiler und Flags
CC = gcc
CFLAGS = -Wall -Wno-multichar -std=gnu17

# Ziele (Executables)
TARGET_SERVER = fs_server
TARGET_CLIENT = fs_client

# Quell- und Objektdateien
SERVER_SRCS = server.c sockaddr_array.c
SERVER_OBJS = $(SERVER_SRCS:.c=.o)

CLIENT_SRCS = client.c sockaddr_array.c
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

# Standard-Target: baut beide Programme
all: $(TARGET_SERVER) $(TARGET_CLIENT)

# --- Server ---
$(TARGET_SERVER): $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# --- Client ---
$(TARGET_CLIENT): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Kompilierschritt für einzelne .c-Dateien
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- Hilfs-Targets ---

# Nur den Server bauen
server: $(TARGET_SERVER)

# Nur den Client bauen
client: $(TARGET_CLIENT)

# Aufräumen
clean:
	rm -f $(SERVER_OBJS) $(CLIENT_OBJS) $(TARGET_SERVER) $(TARGET_CLIENT)

# Startbefehle
run-server: $(TARGET_SERVER)
	./$(TARGET_SERVER)

run-client: $(TARGET_CLIENT)
	./$(TARGET_CLIENT)

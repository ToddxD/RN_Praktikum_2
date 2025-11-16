#include "sockaddr_array.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

// Struktur: IPv4-Adresse + zugehöriger Socket-FD
typedef struct {
	struct sockaddr_in addr;
	int fd;
} sockaddr_entry_t;

static sockaddr_entry_t addr_array[MAX_ADDRS];
static int addr_count = 0;

/**
 * Fügt eine neue IPv4-Adresse mit zugehörigem FD hinzu.
 * Gibt 0 bei Erfolg, -1 bei Fehler (kein Platz).
 */
int add_sockaddr(struct sockaddr_in addr, int fd) {
	if (addr_count >= MAX_ADDRS) {
		fprintf(stderr, "Fehler: Adressarray ist voll.\n");
		return -1;
	}
	addr_array[addr_count].addr = addr;
	addr_array[addr_count].fd = fd;
	addr_count++;
	return 0;
}

/**
 * Entfernt einen Eintrag anhand des File Descriptors.
 * Gibt 0 bei Erfolg, -1 wenn nicht gefunden.
 */
int remove_sockaddr(int fd) {
	for (int i = 0; i < addr_count; i++) {
		if (addr_array[i].fd == fd) {
			// Lücke schließen durch Verschieben
			for (int j = i; j < addr_count - 1; j++) {
				addr_array[j] = addr_array[j + 1];
			}
			addr_count--;
			return 0;
		}
	}
	fprintf(stderr, "FD %d nicht gefunden.\n", fd);
	return -1;
}

/**
 * Gibt alle gespeicherten IPv4-Adressen und FDs als String aus.
 */
void string_sockaddrs(char *str) {
	for (int i = 0; i < addr_count; i++) {
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr_array[i].addr.sin_addr, ip, INET_ADDRSTRLEN);

		char formatted[INET_ADDRSTRLEN + 10];
		sprintf(formatted, "%s:%u\r\n", ip, ntohs(addr_array[i].addr.sin_port));
		strcat(str, formatted);
	}
	strcat(str, "\r\n");

	char count[10];
	sprintf(count, "%d clients", addr_count);
	strcat(str, count);
}

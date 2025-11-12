#include "sockaddr_array.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

static struct sockaddr_in addr_array[MAX_ADDRS];
static int addr_count = 0;

/**
 * Vergleicht zwei IPv4-Adressen.
 * Gibt 1 zurück, wenn sie gleich sind, sonst 0.
 */
static int compare_sockaddr(struct sockaddr_in a, struct sockaddr_in b) {
	return (a.sin_family == b.sin_family) &&
		   (a.sin_addr.s_addr == b.sin_addr.s_addr) &&
		   (a.sin_port == b.sin_port);
}

/**
 * Fügt eine neue IPv4-Adresse hinzu.
 * Gibt 0 bei Erfolg, -1 bei Fehler (kein Platz).
 */
int add_sockaddr(struct sockaddr_in addr) {
	if (addr_count >= MAX_ADDRS) {
		fprintf(stderr, "Fehler: Adressarray ist voll.\n");
		return -1;
	}
	addr_array[addr_count++] = addr;
	return 0;
}

/**
 * Entfernt eine IPv4-Adresse, wenn sie existiert.
 * Gibt 0 bei Erfolg, -1 wenn nicht gefunden.
 */
int remove_sockaddr(struct sockaddr_in addr) {
	for (int i = 0; i < addr_count; i++) {
		if (compare_sockaddr(addr_array[i], addr)) {
			// Lücke schließen durch Verschieben
			for (int j = i; j < addr_count - 1; j++) {
				addr_array[j] = addr_array[j + 1];
			}
			addr_count--;
			return 0;
		}
	}
	fprintf(stderr, "Adresse nicht gefunden.\n");
	return -1;
}

/**
 * Gibt alle gespeicherten IPv4-Adressen aus.
 */
void string_sockaddrs(char *str) {
	for (int i = 0; i < addr_count; i++) {
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr_array[i].sin_addr, ip, INET_ADDRSTRLEN);
		char formatted[INET_ADDRSTRLEN + 10];
		sprintf(formatted, "%s:%u\r\n", ip, ntohs(addr_array[i].sin_port));
		strcat(str, formatted);
	}
	strcat(str, "\r\n");

	char count[9];
	sprintf(count, "%d client", addr_count);
	strcat(str, count);
}

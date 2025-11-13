#ifndef SOCKADDR_ARRAY_H
#define SOCKADDR_ARRAY_H

#include <netinet/in.h> // für struct sockaddr_in

#define MAX_ADDRS 10

// --- Funktionsprototypen ---
int add_sockaddr(struct sockaddr_in addr);
int remove_sockaddr(struct sockaddr_in addr);
void string_sockaddrs(char* str);

#endif // SOCKADDR_ARRAY_H

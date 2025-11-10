#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define SERVER "127.0.0.1"
#define PORT 6969

int main() {
  printf("tes1t\n");

  // TCP Socket anlegen
  struct sockaddr_in socket_address_server;
  memset(&socket_address_server, 0, sizeof(socket_address_server));
  socket_address_server.sin_family = AF_INET;
  socket_address_server.sin_port = htons(PORT);
  socket_address_server.sin_addr.s_addr = inet_addr(SERVER);


  int sock = socket(AF_INET, SOCK_STREAM, 0); // vllt SOCK_STREAM|SOCK_NONBLOCK

  int bindCheck = bind(sock, (struct sockaddr *)&socket_address_server, sizeof(socket_address_server));
  listen(sock, 100);
  socklen_t laenge = sizeof(socket_address_server);
  int akzeptierterSocket = accept(sock, (struct sockaddr *)&socket_address_server, &laenge);



  int sock2 = socket(AF_INET, SOCK_STREAM, 0);
  int connectionCheck = connect(sock2, (struct sockaddr *)&socket_address_server, sizeof(socket_address_server));

  return 0;
}


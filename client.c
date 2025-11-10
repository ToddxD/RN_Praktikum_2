#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER "127.0.0.1"
#define PORT 6969

#define MAX_LEN 1500
#define READ_BUF_SIZE 1500

int main() {
  struct sockaddr_in socket_address_server;
  memset(&socket_address_server, 0, sizeof(socket_address_server));
  socket_address_server.sin_family = AF_INET;
  socket_address_server.sin_port = htons(PORT);
  socket_address_server.sin_addr.s_addr = inet_addr(SERVER);

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (connect(sock, (struct sockaddr *)&socket_address_server, sizeof(socket_address_server)) < 0) {
    perror("Fehler beim Connect");
    return -1;
  }

  // clients\n\004
  // files\n\004
  // get datei.txt\n\004
  // put datei.txt\n\nINHALT\004
  // quit\n\004

  char* buff = malloc(MAX_LEN * sizeof(char));
  char* paket = malloc((MAX_LEN + 3) * sizeof(char));
  while(1) {
    memset(buff, 0, MAX_LEN);
    int i = 0;
    while(1) {
      char ch = getchar();
      if (ch == '#' || i >= MAX_LEN) {
        break;
      }
      if (ch == '\n') { // Für CR LF muss hier \r zusätzlich hinzugefügt werden.
        if (i == 0) {
          continue;
        }
        buff[i] = '\r';
        i++;
      }
      buff[i] = ch;
      i++;
    }


    if (strcmp(buff, "quit") == 0) {
      //shutdown(sock, SHUT_WR);
      close(sock);
      printf("[Client] closed\n");
      return 0;
    }

    strcpy(paket,buff);
    strcat(paket,"\r\n\004");

    if (write(sock, paket, strlen(paket)) < 0) {
      perror("Fehler beim Senden des Pakets");
      return -1;
    }
    printf("[Client] paket sent: %s\n", paket);

    char* buf = calloc(READ_BUF_SIZE, sizeof(char)); // ggf. auf MTU Size anpassen?
    ssize_t count = read(sock, buf, READ_BUF_SIZE);
    printf("%s", buf);
  }
}
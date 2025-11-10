#include <stdbool.h>
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
#include <sys/epoll.h>
#include <sys/stat.h>
#include <time.h>
#include <dirent.h>

#define SERVER "127.0.0.1"
#define PORT 6969

#define READ_BUF_SIZE 1500

#define MAX_EVENTS 10 // Wie viele Clients gleichzeit kommunizieren können

#define STATUS_OK "OK"

int str_starts(const char* str, const char* prefix) {
  return !strncmp(str, prefix, strlen(prefix));
}

int str_ends(const char* str, const char* suffix) {
  return !strcmp(str + strlen(str) - strlen(suffix), suffix);
}

void read_conn(const struct epoll_event *event) {
  int done = 0;
  int connection = event->data.fd;

  printf("[Server] message start:\n");
  while(1) {

    char* buf = calloc(READ_BUF_SIZE, sizeof(char)); // ggf. auf MTU Size anpassen?
    ssize_t count = read(connection, buf, READ_BUF_SIZE);

    if (count < 0) {
      // If errno == EAGAIN, that means we have read all data. So go back to the main loop.
      if (errno != EAGAIN) {
        perror ("[Server] read");
        done = true;
      }
      break;
    } else if (count == 0) {
      done = true;
      break;
    } else {
      //if (count < READ_BUF_SIZE) {
      //  buf[count] = '\0'; // Ende abschneiden
      //}

      if(strcmp(buf, "clients\r\n\004") == 0) {

      } else if(strcmp(buf, "files\r\n\004") == 0) {
        DIR *directory;
        struct dirent *ent;
        if ((directory = opendir (".")) != NULL) {
          while ((ent = readdir (directory)) != NULL) {
            struct stat st;
            stat(ent->d_name, &st);
            printf ("%s\n", ent->d_name);
            printf("%ld\n", st.st_size);
          }
          closedir (directory);

        }
        return;

      } else if(strncmp(buf, "get", 3) == 0){
        char fileName[50];
        char content[1000];
        char* firstSpace = strchr(buf, ' ');
        char* firstLineBreak = strchr(buf, '\r\n');
        strncpy(fileName, firstSpace+1, firstLineBreak-buf-5);
        FILE *fptr;
        fptr = fopen(fileName, "r");
        fgets(content, 1000, fptr);
        fclose(fptr);
        write(connection, content, 1000);

      } else if(strncmp(buf, "put", 3) == 0){
        char fileName[50];
        char content[1000];
        char* firstSpace = strchr(buf, ' ');
        char* firstLineBreak = strchr(buf, '\r\n');
        char* eOT = strchr(buf, '\004');
        strncpy(fileName, firstSpace+1, firstLineBreak-buf-5);
        strncpy(content, firstLineBreak+3, eOT-firstLineBreak-5);
        FILE *fptr;
        fptr = fopen(fileName, "w");
        fprintf(fptr, "%s", content);
        fclose(fptr);
        printf("[Server] Fertig geschrieben!\n");

        char tim[20];
        char ret[40];
        time_t now = time(NULL);
        strftime(tim, 20, "%X", localtime(&now));
        sprintf(ret, "%s\r\n%s\r\n\004", STATUS_OK, tim);
        write(connection, ret, strlen(ret));
        return;

      } else if(strcmp(buf, "quit\n\004") == 0){

      }
      printf("%s", buf);
      fflush(stdout);
      usleep(1000*500);
    }
  }

  if (done) {
    printf("\n[Server] message end\n");
    //close (connection);
  }
}

int main(int argc, char **argv) {
  int index;
  int c;
  int port = 6969;
  int storage_size_limit;
  opterr = 0;
  char *dir = "./store";
  struct stat st = {0};
/*
  //parse arguments
  while((c = getopt(argc,argv,"p:s:1:h"))!= 1)
    switch(c){
    case 'p':
      port = (int) strtol(optarg,NULL,10);
      break;
    case 's':
      if( optarg[0] != '.' ){
        printf("Incorrect directory syntax!\n");
        break;
      }
      dir = optarg;
      dir_set = 1;
      break;
    case '1':
      storage_size_limit = (int) strtol(optarg,NULL,10);
      break;
    case 'h':
      printf("Usage: fs_server [OPTIONS . . . ]\n"
             " where\n"
             " OPTIONS := { -p port | -s storage_location | -1\n"
             " storage_size_limit | -h[elp] }\n");
      break;
    case '?':
      fprintf(stderr, "Unknown Option '-%c'.\n",optopt);
      return 1;
    default:
  }

  //create storage directory, if possible
  if(stat(dir,&st) == -1) {
    mkdir(dir, 0700);
  }
*/

  // TCP Socket anlegen
  struct sockaddr_in socket_address_server;
  memset(&socket_address_server, 0, sizeof(socket_address_server));
  socket_address_server.sin_family = AF_INET;
  socket_address_server.sin_port = htons(PORT);
  socket_address_server.sin_addr.s_addr = inet_addr(SERVER);


  int sock = socket(AF_INET, SOCK_STREAM, 0); // vllt SOCK_STREAM|SOCK_NONBLOCK

  int one = 1;
  const int *val = &one;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, val, sizeof(one)) < 0) {
    perror("[Server] error setsockopt IP_HDRINCL");
    return -1;
  }

  if(bind(sock, (struct sockaddr *)&socket_address_server, sizeof(socket_address_server)) < 0){
    perror("[Server] bind error");
    return -1;
  }

  if (listen(sock, 100) < 0) {
    perror("[Server] listen error");
  }


  struct epoll_event events[MAX_EVENTS]; // = calloc(maxEventCount, sizeof());
  int epoll = epoll_create1(0);
  if (epoll < 0) {
    perror("[Server] error creating epoll");
  }

  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = sock;

  // Socket zur epoll Liste hinzufügen
  if (epoll_ctl(epoll, EPOLL_CTL_ADD, sock, &event) < 0) {
    perror("[Server] error adding socket");
  }

  while (1) {
    int event_count = epoll_wait(epoll, events, MAX_EVENTS, -1);

    for (int i = 0; i < event_count; i++) {
      if ((events[i].events & EPOLLERR) ||
          (events[i].events & EPOLLHUP) ||
          (!(events[i].events & EPOLLIN)))
      {
        printf("[Server] epoll error\n");
        close(events[i].data.fd);
      } else if (events[i].data.fd == sock) { // Event kommt vom Socket
        socklen_t laenge = sizeof(socket_address_server);
        // Handshake
        int connection = accept(sock, (struct sockaddr *)&socket_address_server, &laenge);

        if (connection < 0) {
          perror("[Server] accept error");
        }

        // Die Verbindung (filedescr.) zum Client zusätzlich in die Liste packen
        event.data.fd = connection;
        event.events = EPOLLIN;
        if (epoll_ctl(epoll, EPOLL_CTL_ADD, connection, &event) < 0) {
          perror("[Server] couldn't add connection");
        }
        printf("[Server] added connection to epoll\n");

      } else { // Wenn das Event von der Verbindung und nicht vom socket kommt, dann kann gelesen werden
        read_conn(events + i);

        //close(sock);
        //return 0;
      }
    }

  }

  close(sock);
  return 0;
}
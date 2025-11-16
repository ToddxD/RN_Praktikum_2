#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <signal.h>
#include <sys/epoll.h>

#define MAX_EVENTS 5

int tcp_server_fd = -1;
int tcp_client_fd = -1;

static void sigint_handler(int signo)
{
  (void)close(tcp_server_fd);
  (void)close(tcp_client_fd);
  sleep(2);
  printf("Caught sigINT!\n");
  exit(EXIT_SUCCESS);
}

void register_signal_handler(
        int signum,
        void (*handler)(int))
{
  if (signal(signum, handler) == SIG_ERR) {
    printf("Cannot handle signal\n");
    exit(EXIT_FAILURE);
  }
}

void validate_convert_port(
        char *port_str,
        struct sockaddr_in *sock_addr)
{
  int port;

  if (port_str == NULL) {
    perror("Invalid port_str\n");
    exit(EXIT_FAILURE);
  }

  if (sock_addr == NULL) {
    perror("Invalid sock_addr\n");
    exit(EXIT_FAILURE);
  }

  port = atoi(port_str);

  if (port == 0) {
    perror("Invalid port\n");
    exit(EXIT_FAILURE);
  }

  sock_addr->sin_port = htons(
          (uint16_t)port);
  printf("Port: %d\n",
         ntohs(sock_addr->sin_port));
}

void recv_send(char *buffer)
{
  int len, ret;

  memset(buffer, 0,
         sizeof(buffer));
  len = recv(tcp_client_fd, buffer,
             sizeof(buffer) - 1, 0);

  if (len > 0) {
    buffer[len] = '\0';
    printf("Received: %s\n",
           buffer);

    memset(buffer, 0,
           sizeof(buffer));
    strncpy(buffer, "HELLO",
            strlen("HELLO") + 1);
    buffer[strlen(buffer) + 1] = '\0';
    printf("Sentbuffer = %s\n",
           buffer);

    ret = send(tcp_client_fd, buffer,
               strlen(buffer), 0);

    if (ret < 0) {
      perror("send error\n");
      (void)close(tcp_client_fd);
      (void)close(tcp_server_fd);
      exit(EXIT_FAILURE);
    }

  } else if (len < 0) {
    perror("recv");
    (void)close(tcp_client_fd);
    (void)close(tcp_server_fd);
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char *argv[])
{
  int epoll_fd;
  int ready_fds;
  int ret;
  struct sockaddr_in
          tcp_addr;
  char buffer[1024];
  struct epoll_event
          events[MAX_EVENTS];

  socklen_t tcp_addr_len = sizeof(
          tcp_addr);

  register_signal_handler(SIGINT,
                          sigint_handler);

  if (argc != 2) {
    printf("%s <port-number>",
           argv[0]);
    return -1;
  }

  memset(&tcp_addr, 0,
         sizeof(tcp_addr));
  tcp_addr.sin_family = AF_INET;
  tcp_addr.sin_addr.s_addr =
          INADDR_ANY;
  validate_convert_port(argv[1],
                        &tcp_addr);

  tcp_server_fd = socket(AF_INET,
                         SOCK_STREAM,
                         IPPROTO_TCP);

  if (tcp_server_fd < 0) {
    perror("socket");
    return -2;
  }

  ret = bind(tcp_server_fd,
             (struct sockaddr *)&tcp_addr,
             sizeof(tcp_addr));

  if (ret < 0) {
    perror("bind");
    (void)close(tcp_server_fd);
    return -3;
  }

  ret = listen(tcp_server_fd,
               MAX_EVENTS);

  if (ret < 0) {
    perror("listen");
    (void)close(tcp_server_fd);
    return -4;
  }

  printf("Server is listening\n");

  tcp_client_fd = accept(tcp_server_fd,
                         (struct sockaddr *) &tcp_addr,
                         &tcp_addr_len);

  if (tcp_client_fd < 0) {
    perror("accept");
    (void)close(tcp_server_fd);
    return -5;
  }

  printf("Connection accepted\n");

  epoll_fd = epoll_create1(0);

  if (epoll_fd <= 0)
  {
    perror("Epoll creation failed");
    exit(EXIT_FAILURE);
  }

  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = tcp_client_fd;

  ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD,
                  tcp_client_fd, &event);

  if (ret  == -1) {
    perror("Epoll_ctl failed");
    exit(EXIT_FAILURE);
  }

  while (1) {
    ready_fds = epoll_wait(epoll_fd,
                           events,
                           MAX_EVENTS, -1);

    if (ready_fds < 0) {
      perror("Epoll wait failed");
      break;
    }

    if (events[0].data.fd ==
        tcp_client_fd) {
      recv_send(buffer);
    }
  }

  (void)close(tcp_client_fd);
  (void)close(tcp_server_fd);

  return 0;
}

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

#define MAX_LEN 1500
#define READ_BUF_SIZE 1500

int dflag = 0;

void parse_args(int argc, char **argv){
    int c;
    opterr = 0;

    while(( c = getopt(argc,argv,"hd" )) != -1){
        switch(c) {
            case 'h':
                printf("Usage : fs_client [ OPTIONS ...] SERVER\n"
                       "where\n"
                       "  OPTIONS := { -h [ elp ] }\n"
                       "  SERVER := { < address >: < port > | < name >: < port > }\n");
                exit(0);
            case 'd':
                dflag = 1;
                printf("Default IP used\n");
                break;
            case '?':
                fprintf(stderr,"Unknown option '-%c'. Use -h for help.\n",optopt);
                exit(-1);
            default:
                break;
        }
    }
}

int main(int argc, char **argv) {
    char ip_str[64];
    int port;

    parse_args(argc,argv);

    if (optind >= argc) {
        fprintf(stderr, "Error: Too few arguments. Use -h for help.\n");
        return EXIT_FAILURE;
    }

    char *arg = argv[optind];
    char *colon = strchr(arg, ':');
    if (!colon) {
        fprintf(stderr, "Error: Unexpected argument. Use -h for help.\n");
        return EXIT_FAILURE;
    }

    printf("Connecting to Server %s ...\n", argv[optind]);

    size_t ip_len = colon - arg;
    strncpy(ip_str, arg, ip_len);
    ip_str[ip_len] = '\0';
    port = (int) strtol(colon + 1,NULL,10);

    //printf("IP: %s Port: %d\n",ip_str,port);

    struct sockaddr_in socket_address_server;
    memset(&socket_address_server, 0, sizeof(socket_address_server));
    socket_address_server.sin_family = AF_INET;
    socket_address_server.sin_port = htons(port);
    socket_address_server.sin_addr.s_addr = inet_addr(ip_str);

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
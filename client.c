#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <regex.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_LEN 1500
#define BUF_SIZE 1500
#define REGEX_PUT "put[[:space:]][a-zA-Z0-9_-]*\\.txt\\r\\n\\r\\n[\\0-\\377]*\\r\\n\\004.*"

int dflag = 0;

void parse_args(int argc, char **argv) {
	int c;
	opterr = 0;

	while ((c = getopt(argc, argv, "hd")) != -1) {
		switch (c) {
			case 'h':
				printf("Usage : fs_client [OPTIONS ...] SERVER\n"
					   "where\n"
					   "  OPTIONS := { -h[elp] }\n"
					   "  SERVER := { <address> : <port> | <name> : <port> }\n");
				exit(0);
			case 'd': //just for debug
				dflag = 1;
				printf("using default IP-Address\n");
				break;
			case '?':
				fprintf(stderr, "Unknown option '-%c'. Use -h for help.\n", optopt);
				exit(-1);
			default:
				break;
		}
	}
}

int main(int argc, char **argv) {
	char ip_str[64];
	int port;

	parse_args(argc, argv);

	if (optind >= argc) {
		fprintf(stderr, "Error: Unexpected or no arguments. Use -h for help.\n");
		return EXIT_FAILURE;
	}

	char *arg = argv[optind];
	char *colon = strchr(arg, ':');
	if (!colon) {
		fprintf(stderr, "Error: Unexpected arguments. Use -h for help.\n");
		return EXIT_FAILURE;
	}

	printf("Connecting to Server %s ...\n", argv[optind]);

	//Get IP-Address
	size_t ip_len = colon - arg;
	strncpy(ip_str, arg, ip_len);
	ip_str[ip_len] = '\0';

	//get Port
	port = (int) strtol(colon + 1, NULL, 10);

	//TCP Socket
	struct sockaddr_in socket_address_server;
	memset(&socket_address_server, 0, sizeof(socket_address_server));
	socket_address_server.sin_family = AF_INET;
	socket_address_server.sin_port = htons(port);
	socket_address_server.sin_addr.s_addr = inet_addr(ip_str);

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (connect(sock, (struct sockaddr *) &socket_address_server, sizeof(socket_address_server)) < 0) {
		perror("Fehler beim Connect");
		return -1;
	}

	while (1) {
		char *buf = NULL;
		size_t size = 0;
		FILE *in_stream = open_memstream(&buf, &size);

		printf("[Client] enter command:\n");
		int i = 0;
		while (1) {
			char ch = getchar();
			if (ch == '#' || i >= (MAX_LEN - 3)) {
				break;
			}
			if (ch == '\n') { // Für CR LF muss hier \r zusätzlich hinzugefügt werden.
				if (i == 0) {
					continue;
				}
				fputc('\r', in_stream);
				i++;
			}
			fputc(ch, in_stream);
			i++;
		}

		fwrite("\r\n\004", sizeof(char), 4, in_stream); // EOT hinzufügen
		fclose(in_stream);
		
		if (strcmp(buf, "quit\r\n\004") == 0) {
			write(sock, buf, strlen(buf));
			close(sock);

			printf("[Client] closed\n");
			free(buf);
			return 0;
		}

		// Sende Anfrage
		size_t total_sent = 0;
		char *offset = buf;
		while (total_sent < size) {
			size_t n = write(sock, offset + total_sent, BUF_SIZE);
			if (n < 0) {
				perror("[Server] error sending file data");
				free(buf);
				return -1;
			}
			total_sent += n;
		}
		
		printf("[Client] paket sent: %s\n", buf);
		printf("[Client] answer:\n");

		// Lese Antwort
		char *antwort_buf = calloc(BUF_SIZE, sizeof(char));
		while (1) {
			read(sock, antwort_buf, BUF_SIZE);
			printf("%s\n", antwort_buf);

			char *eot = memchr(antwort_buf, '\004', BUF_SIZE); // EOT finden und abbrechen
			if (eot) {
				break;
			}
		}

		printf("[Client] paket received\n");
		free(antwort_buf);
		free(buf);
	}
}
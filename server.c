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
#include <regex.h>
#include "sockaddr_array.h"
#include <sys/sendfile.h>
#include <fcntl.h>

#define SERVER "127.0.0.10"

#define BUF_SIZE 1500

#define MAX_EVENTS 10 // Wie viele Clients gleichzeitig kommunizieren können

#define STATUS_OK "OK"
#define REGEX_PUT "[^~./]+"
#define REGEX_GET "get [a-zA-Z0-9_-]+\\.txt\\r\\n\\004"
#define RESP_FNOTFOUND "Datei nicht gefunden, bitte Namen überprüfen\r\n\004"

char *patternPut = "^[).,_a-zA-Z0-9(-]+$";
regex_t regexGet;
regex_t regexPut;

long long storage_size_limit = 10 * 1000000000;
char *dir = "./store";


int testDateiNameAufEndung(char *fileName) {
	int laenge = strlen(fileName);
	if (fileName[laenge - 4] != '.' || fileName[laenge - 3] != 't' || fileName[laenge - 2] != 'x' ||
		fileName[laenge - 1] != 't') {
		printf("falsches Dateiformat");
	} else {
		printf("Korrekt: %s\n", fileName);
	}
	return laenge;
}

void cmd_clients(int connection)
{
    char ret[BUF_SIZE] = {0};
    string_sockaddrs(ret);
    strcat(ret, "\r\n\004");
    write(connection, ret, sizeof(ret));
}

void cmd_files(int connection) {
	DIR *directory;
	struct dirent *ent;
	char ret[BUF_SIZE] = {0};

	if ((directory = opendir(dir)) != NULL) {
		while ((ent = readdir(directory)) != NULL) {
			struct stat st = {0};
			char *filename = ent->d_name;
			char fileNamefinal[200] = {0};
			sprintf(fileNamefinal, "%s/%s", dir, filename);
			char *realFileName = realpath(filename, NULL);
			stat(fileNamefinal, &st);

			char tim[30] = {0};
			strftime(tim, sizeof(tim), "%x-%X", localtime(&st.st_ctime));

			char formatted[sizeof(tim) + sizeof(filename) + 10] = {0};
			sprintf(formatted, "%s %s,%ldB\r\n", filename, tim, st.st_size);
			strcat(ret, formatted);
		}
		strcat(ret, "\r\n\004");
		write(connection, ret, sizeof(ret));
		closedir(directory);
	}
}

void cmd_get(int connection, char *buf) {
	// filename extrahieren
	char fileName[100] = {0};
	char *firstSpace = strchr(buf, ' ');
	char *firstLineBreak = strchr(buf, '\r\n');

	if (firstSpace == NULL || firstLineBreak == NULL) {
		char *ret = "invalid command\r\n\004";
		write(connection, ret, strlen(ret));
		return;
	}

	int fileNameLen = firstLineBreak - firstSpace - 1;
	strcpy(fileName, dir);
	strcat(fileName, "/");
	strncat(fileName, firstSpace + 1, fileNameLen);
	fileName[strlen(dir) + fileNameLen] = '\0';


	// Zugriffzeitpunkt und Größe ermitteln
	char *ret = NULL;
	size_t size = 0;
	FILE *ret_stream = open_memstream(&ret, &size);
	struct stat st;
	if (stat(fileName, &st) == -1) {
		write(connection, RESP_FNOTFOUND, strlen(RESP_FNOTFOUND));
		fclose(ret_stream);
		free(ret);
		return;
	}
	char tim[30] = {0};
	strftime(tim, sizeof(tim), "%x-%X", localtime(&st.st_mtim.tv_sec));
	fprintf(ret_stream, "%s,%ldB\r\n\r\n", tim, st.st_size);


	// Dateiinhalt einfügen
	char chunk[512];
	size_t n;
	FILE *fp = fopen(fileName, "r");
	if (fp == NULL) {
		write(connection, RESP_FNOTFOUND, strlen(RESP_FNOTFOUND));
		fclose(ret_stream);
		free(ret);
		return;
	}
	while ((n = fread(chunk, sizeof(char), sizeof(chunk), fp)) > 0) {
		fwrite(chunk, sizeof(char), n, ret_stream);
	}
	fclose(fp);
	fprintf(ret_stream, "\004");
	fclose(ret_stream);

	size_t total_sent = 0;
	char *offset = ret;
	while (total_sent < size) {
		size_t n = write(connection, offset + total_sent, BUF_SIZE);
		if (n < 0) {
			perror("[Server] error sending file data");
			free(ret);
			return;
		}
		total_sent += n;
	}
	free(ret);
}

void cmd_put(int connection, char *buf) {
	char fileName[50] = {0};
	char *content = calloc(sizeof(char), BUF_SIZE);
	char *findEOT = memchr(buf, '\004', BUF_SIZE);
	char *firstSpace = strchr(buf, ' ');
	char *firstLineBreak = strchr(buf, '\r\n');
	strncpy(fileName, firstSpace + 1, firstLineBreak - buf - 5);
	//Prüfen, ob Dateiname erlaubt ist, falls nicht wird geantwortet, dass der Dateiname ungültig ist
	int dateiNameTest = regexec(&regexPut, fileName, 0, NULL, 0);
	if (dateiNameTest == 0) {
		//Absoluten Pfad für die Datei herausfinden
		char *realPathName = realpath(dir, NULL);
		strcat(realPathName, "/");
		strcat(realPathName, fileName);
		if(findEOT) {
			FILE *fptr = fopen(realPathName, "w");
			const char *eOT = strchr(buf, '\004');
			strncpy(content, firstLineBreak + 3, eOT - firstLineBreak - 5);
			fprintf(fptr, "%s", content);
			fclose(fptr);
		} else {
			strncpy(content, firstLineBreak + 3, BUF_SIZE-strlen(fileName)-4);
			FILE *fptr = fopen(realPathName, "a");
			while(1) {
				fprintf(fptr, "%s", content);
				memset(content, 0, BUF_SIZE);
				ssize_t count = read(connection, content, BUF_SIZE);
				findEOT = strchr(content, '\004');
				if(findEOT) {
					break;
				}
			}
			content[strlen(content)-3] = '\0';
			fprintf(fptr, "%s", content);
			fclose(fptr);
		}

		//Datei anlegen und Inhalt hineinschreiben
		char ret[21] = "OK ";
		time_t now = time(NULL);
		strftime(ret+3, 18, "%x-%X", localtime(&now));
		strcat(ret, "\r\n\004");
		write(connection, ret, strlen(ret));
	} else {
		char ret[22] = "NOK ";
		time_t now = time(NULL);
		strftime(ret+4, 18, "%x-%X", localtime(&now));
		strcat(ret, "\r\n\004");
		write(connection, ret, strlen(ret));
	}
	free(content);
}

void read_conn(const struct epoll_event *event) {
	int connection = event->data.fd;

	char *buf = calloc(BUF_SIZE, sizeof(char)); // ggf. auf MTU Size anpassen?
	ssize_t count = read(connection, buf, BUF_SIZE);
	if (count < 0) {
		if (errno != EAGAIN) {
			perror("[Server] read failed");
			close(connection);
			remove_sockaddr(connection);
		}
	} else if (count == 0) {
		// nothing todo
	} else {
		if (strcmp(buf, "clients\r\n\004") == 0) {
            cmd_clients(connection);
        } else if (strcmp(buf, "files\r\n\004") == 0) {
			cmd_files(connection);
		} else if (strncmp(buf, "get", 3) == 0) {
			cmd_get(connection, buf);
		} else if (strncmp(buf, "put", 3) == 0) {
			cmd_put(connection, buf);
		} else if (strcmp(buf, "quit\r\n\004") == 0) {
			close(connection);
			remove_sockaddr(connection);
		} else {
			printf("%s\n", buf);
			char *ret = "invalid command\r\n\004";
			write(connection, ret, strlen(ret));
		}
	}
	free(buf);
}

long long get_dir_size(const char *path)
{
    struct stat st;
	long long total_size = 0;

	DIR *dir = opendir(path);
	if (!dir) {
		fprintf(stderr, "Can't open directory %s : %s\n", path, strerror(errno));
		return -1;
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		//skip . and ..
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		char fullpath[4096];
		snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

		if (stat(fullpath, &st) == -1) {
			fprintf(stderr, "Error, stat %s: %s\n", fullpath, strerror(errno));
			continue;
		}

		if (S_ISREG(st.st_mode)) {
			total_size += st.st_size;
		}
	}

	closedir(dir);

	return total_size;
}

int main(int argc, char **argv) {

	int retiGet = regcomp(&regexGet, REGEX_GET, REG_EXTENDED);
	if (retiGet != 0) {
		fprintf(stderr, "regcomp failed\n");
	}
	int retiPut = regcomp(&regexPut, patternPut, REG_EXTENDED);
	if (retiPut != 0) {
		fprintf(stderr, "regcomp failed\n");
	}
	int port = 0;
	opterr = 0;
	struct stat st = {0};

	//parse arguments
	int c;
	while ((c = getopt(argc, argv, "p:s:1:h")) != -1)
		switch (c) {
			case 'p':
				port = (int) strtol(optarg, NULL, 10);
				printf("[Server] Port %d selected\n", port);
				break;
			case 's':
				if (optarg[0] != '.') {
					printf("Incorrect directory syntax! Using standard directory.\n");
					break;
				}
				dir = optarg;
				break;
			case '1':
				storage_size_limit = strtoll(optarg, NULL, 10);
				printf("Set storage size limit to %lld\n", storage_size_limit);
				break;
			case 'h':
				printf("Usage: fs_server [OPTIONS . . . ]\n"
					   "where\n"
					   " OPTIONS := { -p port | -s storage_location | -1\n"
					   " storage_size_limit in kB | -h[elp] }\n");
				return 0;
			case '?':
				fprintf(stderr, "Unknown Option '-%c'. Use -h for help.\n", optopt);
				return -1;
			default:
				printf("[Server] Starting server using default configuration.\n");
		}

	//create storage directory
	if (stat(dir, &st) == -1) {
		mkdir(dir, 0700);
	}
	printf("[Server] Directory: %s\n", dir);
	long long dirSize = get_dir_size(dir);

	//check size of directory
	if (dirSize > storage_size_limit) {
		printf("[Server] Directory exceeds size limit, %lld kB > %lld kB\n", dirSize, storage_size_limit);
		return EXIT_FAILURE;
	}

	// TCP Socket anlegen
	struct sockaddr_in socket_address_server;
	memset(&socket_address_server, 0, sizeof(socket_address_server));
	socket_address_server.sin_family = AF_INET;
	socket_address_server.sin_port = htons(port);
	socket_address_server.sin_addr.s_addr = inet_addr(SERVER);

	int sock = socket(AF_INET, SOCK_STREAM, 0); // vllt SOCK_STREAM|SOCK_NONBLOCK

	int one = 1;
	const int *val = &one;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, val, sizeof(one)) < 0) {
		perror("[Server] error setsockopt IP_HDRINCL");
		return -1;
	}

	socklen_t addr_len = sizeof(socket_address_server);
	if (bind(sock, (struct sockaddr *) &socket_address_server, addr_len) < 0) {
		perror("[Server] bind error");
		return -1;
	}

	getsockname(sock, (struct sockaddr *) &socket_address_server, &addr_len);
	printf("[Server] listening on Port: %d\n", ntohs(socket_address_server.sin_port));


	if (listen(sock, 100) < 0) {
		perror("[Server] listen error");
		return -1;
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
				(!(events[i].events & EPOLLIN))) {
				printf("[Server] epoll error\n");
				remove_sockaddr(events[i].data.fd);
				close(events[i].data.fd);
			} else if (events[i].data.fd == sock) { // Event kommt vom Socket
				struct sockaddr_in con_addr;
				socklen_t laenge = sizeof(con_addr);
				// Handshake
				int connection = accept(sock, (struct sockaddr *) &con_addr, &laenge);
				add_sockaddr(con_addr, connection);

				if (connection < 0) {
					perror("[Server] accept error");
				}

				// Die Verbindung (filedescr.) zum Client zusätzlich in die Liste packen
				event.data.fd = connection;
				event.events = EPOLLIN;
				if (epoll_ctl(epoll, EPOLL_CTL_ADD, connection, &event) < 0) {
					perror("[Server] couldn't add connection");
				}

			} else { // Wenn das Event von der Verbindung und nicht vom socket kommt, dann kann gelesen werden
				read_conn(events + i);
			}
		}

	}

	close(sock);
	return 0;
}
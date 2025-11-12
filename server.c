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
#include "sockaddr_array.h"

#define SERVER "127.0.0.10"

#define READ_BUF_SIZE 1500

#define MAX_EVENTS 10 // Wie viele Clients gleichzeitig kommunizieren können

#define STATUS_OK "OK"

void read_conn(const struct epoll_event *event) {
	int connection = event->data.fd;

	printf("[Server] message start:\n");

	char *buf = calloc(READ_BUF_SIZE, sizeof(char)); // ggf. auf MTU Size anpassen?
	ssize_t count = read(connection, buf, READ_BUF_SIZE);

	if (count < 0) {
		// If errno == EAGAIN, that means we have read all data. So go back to the main loop.
		if (errno != EAGAIN) {
			perror("[Server] read");
		}
		return;
	} else if (count == 0) {
		return;
	} else {

		if (strcmp(buf, "clients\r\n\004") == 0) {
			char ret[1500] = {0};
			string_sockaddrs(ret);
			strcat(ret, "\r\n\0004");
			write(connection, ret, sizeof(ret));

			return;

		} else if (strcmp(buf, "files\r\n\004") == 0) {
			DIR *directory;
			struct dirent *ent;
			char ret[1500] = {0};

			if ((directory = opendir(".")) != NULL) {
				while ((ent = readdir(directory)) != NULL) {
					struct stat st;
					char *filename = ent->d_name;
					stat(filename, &st);
					char tim[30] = {0};
					strftime(tim, sizeof(tim), "%x-%X", localtime(&st.st_mtim.tv_sec));
					char formatted[sizeof(tim) + sizeof(filename) + 10] = {0};
					sprintf(formatted, "%s %s,%ldB\r\n", filename, tim, st.st_size);
					strcat(ret, formatted);
				}
				strcat(ret, "\r\n\004");
				write(connection, ret, sizeof(ret));
				closedir(directory);
			}
			return;

		} else if (strncmp(buf, "get", 3) == 0) {
			char fileName[50];
			char content[1000];
			char *firstSpace = strchr(buf, ' ');
			char *firstLineBreak = strchr(buf, '\r\n');
			strncpy(fileName, firstSpace + 1, firstLineBreak - buf - 5);
			FILE *fptr;
			fptr = fopen(fileName, "r");
			fgets(content, 1000, fptr);
			fclose(fptr);
			write(connection, content, 1000);

		} else if (strncmp(buf, "put", 3) == 0) {
			char fileName[50];
			char content[1000];
			char *firstSpace = strchr(buf, ' ');
			char *firstLineBreak = strchr(buf, '\r\n');
			char *eOT = strchr(buf, '\004');
			strncpy(fileName, firstSpace + 1, firstLineBreak - buf - 5);
			strncpy(content, firstLineBreak + 3, eOT - firstLineBreak - 5);
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

		} else if (strcmp(buf, "quit\n\004") == 0) {

		} else {
			// TODO schmeiße Fehler
		}
	}
}

long long get_dir_size(const char *path) {
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
	int port = 0;
	long long storage_size_limit = 10 * 1000;
	opterr = 0;
	char *dir = "./store";
	struct stat st = {0};

	//parse arguments
	int c;
	while ((c = getopt(argc, argv, "p:s:1:h")) != -1)
		switch (c) {
			case 'p':
				port = (int) strtol(optarg, NULL, 10);
				printf("Port %d selected\n", port);
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
				printf("Starting server using default configuration.\n");
		}

	//create storage directory
	if (stat(dir, &st) == -1) {
		mkdir(dir, 0700);
	}

	long long dirSize = get_dir_size(dir);

	//check size of directory
	if (dirSize > storage_size_limit) {
		printf("Directory exceeds size limit, %lld kB > %lld kB\n", dirSize, storage_size_limit);
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
	printf("Server listening on Port: %d\n", ntohs(socket_address_server.sin_port));


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
				close(events[i].data.fd);
			} else if (events[i].data.fd == sock) { // Event kommt vom Socket
				struct sockaddr_in con_addr;
				socklen_t laenge = sizeof(con_addr);
				// Handshake
				int connection = accept(sock, (struct sockaddr *) &con_addr, &laenge);
				add_sockaddr(con_addr); // TODO Errorhandling

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
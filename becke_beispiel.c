#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#include <arpa/inet.h>
#include <unistd.h>

/*struct iphdr {
    unsigned int ihl:4;
    unsigned int version:4;
    uint8_t tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
    /* Die Optionen und Daten folgen hier.
};*/


unsigned short calculate_checksum(unsigned short *pAddress, int len) {
  int nLeft = len;
  int sum = 0;
  unsigned short *p = pAddress;
  unsigned short answer = 0;

  while (nLeft > 1) {
    sum += *p++;
    nLeft -= 2;
  }

  if (nLeft == 1) {
    *(unsigned char *) (&answer) = *(unsigned char *) p;
    sum += answer;
  }

  sum = (sum >> 16) + (sum & 0xFFFF);
  sum += (sum >> 16);
  answer = ~sum;

  return answer;
}

int main() {
  int s;
  struct sockaddr_in sin;
  char packet[1024];
  struct iphdr *ip = (struct iphdr *) packet;
  memset(packet, 0, sizeof(packet));
  printf("Trace on Loopback\n");
  // Erstellen eines RAW-Sockets
  s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if (s == -1) {
    // Fehlerbehandlung
    perror("Fehler beim Erstellen des Sockets");
    return -1;
  }

  // Socket-Optionen setzen, falls notwendig
  //
  int one = 1;
  const int *val = &one;

  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, val, sizeof(one)) < 0) {
    perror("Fehler beim Setzen von IP_HDRINCL");
    return -1;
  }

  // Zieladresse konfigurieren
  sin.sin_family = AF_INET;
  sin.sin_port = htons(0);
  sin.sin_addr.s_addr = inet_addr("127.0.0.1"); // Ziel-IP-Adresse

  // IP-Header konfigurieren
  ip->ihl = 5;
  ip->version = 4;
  ip->tos = 16;
  ip->tot_len = sizeof(struct iphdr); // Gesamtgröße des Pakets
  ip->id = htons(54321);
  ip->frag_off = 0;
  ip->ttl = 64;
  ip->protocol = IPPROTO_TCP; // oder ein anderes Protokoll
  ip->check = 0; // Setze Checksumme zunächst auf 0
  ip->saddr = inet_addr("8.8.8.8"); // Setzen der Quell-IP-Adresse auf 8.8.8.8
  ip->daddr = sin.sin_addr.s_addr;

  // Checksumme berechnen
  ip->check = calculate_checksum((unsigned short *)packet, ip->tot_len);


  // Paket senden
  if (sendto(s, packet, sizeof(packet), 0, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    perror("Fehler beim Senden des Pakets");
    return -1;
  }

  printf("Paket erfolgreich gesendet\n");

  close(s);
  return 0;
}


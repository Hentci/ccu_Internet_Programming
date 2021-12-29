#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <pcap.h>

#ifndef structOmit
typedef struct pcap_pkthdr pcap_pkthdr;
typedef struct ether_header ether_header;
typedef struct tcphdr tcphdr;
typedef struct udphdr udphdr;
typedef struct ip ip;
#endif

// Media Access Control Address
#define MaxMACLEN 20



int main(int argc, char **argv){




    return 0;
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
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
typedef struct ip6_hdr ip6_hdr;
#endif

// Media Access Control Address
#define MaxMACLEN 20

void separateLine(char *str) {
    printf("\n");
    int lineLen = 56, Len = strlen(str), leftBound = (lineLen - Len) / 2, rightBound = lineLen - Len - leftBound;
    for(int i = 0;i < leftBound;i++) printf("━");
    printf("\n ↆ %s ↆ \n", str);
    for(int i = 0;i < rightBound;i++) printf("━");
    printf("\n");
    printf("\n");
}

char *MAC_ntoa(u_char *num) {
    char *MAC = malloc(sizeof(char) * MaxMACLEN);
    for(int i = 0;i < 6;i++) {
        char *tmp = malloc(sizeof(char) * 2);
        sprintf(tmp, "%02x", num[i]); // %02x => to hex and if number is not > 10, print zero before it
        if(i) strcat(MAC, ":");
        strcat(MAC, tmp);
        free(tmp);
    }
    return MAC;
}

void TCP_or_UDP(ip *ip_header, u_char *packet){
    tcphdr *tcp_header;
    udphdr *udp_header;
    if(ip_header->ip_p == IPPROTO_UDP) {
        udp_header = (udphdr *)(packet + ETHER_HDR_LEN + ip_header->ip_hl * 4);
        printf("┌──────────────────┬────────┐\n");
        printf("│ Protocol         │    UDP │\n"); 
        printf("├──────────────────┼────────┤\n");
        printf("│ Sourse Port      │ %6d │\n", ntohs(udp_header->uh_sport));       
        printf("├──────────────────┼────────┤\n");
        printf("│ Destination Port │ %6d │\n", ntohs(udp_header->uh_dport));       
        printf("└──────────────────┴────────┘\n");
    } else if(ip_header->ip_p == IPPROTO_TCP) {
        tcp_header = (tcphdr *)(packet + ETHER_HDR_LEN + ip_header->ip_hl * 4);
        printf("┌──────────────────┬────────┐\n");
        printf("│ Protocol         │    TCP │\n");       
        printf("├──────────────────┼────────┤\n");
        printf("│ Sourse Port      │ %6d │\n", ntohs(tcp_header->th_sport));       
        printf("├──────────────────┼────────┤\n");
        printf("│ Destination Port │ %6d │\n", ntohs(tcp_header->th_dport));       
        printf("└──────────────────┴────────┘\n");
    }
}

void TCP_or_UDP_for_IPv6(ip6_hdr *ip6_header, u_char *packet){
    tcphdr *tcp_header;
    udphdr *udp_header;
    if(ip6_header->ip6_nxt == IPPROTO_UDP) {
        udp_header = (udphdr *)(packet + ETHER_HDR_LEN + sizeof(struct ip6_hdr));
        printf("┌──────────────────┬────────┐\n");
        printf("│ Protocol         │    UDP │\n"); 
        printf("├──────────────────┼────────┤\n");
        printf("│ Sourse Port      │ %6d │\n", ntohs(udp_header->uh_sport));       
        printf("├──────────────────┼────────┤\n");
        printf("│ Destination Port │ %6d │\n", ntohs(udp_header->uh_dport));       
        printf("└──────────────────┴────────┘\n");
    } else if(ip6_header->ip6_nxt == IPPROTO_TCP) {
        tcp_header = (tcphdr *)(packet + ETHER_HDR_LEN + sizeof(struct ip6_hdr));
        printf("┌──────────────────┬────────┐\n");
        printf("│ Protocol         │    TCP │\n");       
        printf("├──────────────────┼────────┤\n");
        printf("│ Sourse Port      │ %6d │\n", ntohs(tcp_header->th_sport));       
        printf("├──────────────────┼────────┤\n");
        printf("│ Destination Port │ %6d │\n", ntohs(tcp_header->th_dport));       
        printf("└──────────────────┴────────┘\n");
    }
}

int main(int argc, char **argv){
    if(argc < 2){
        printf("./PacketAnalyzer.out <filename>\n");
        exit(0);
    }
    
    char *filename;
    filename = strdup(argv[argc - 1]);

    char errbuff[PCAP_ERRBUF_SIZE] = "\0";

    pcap_t *handler = pcap_open_offline(filename, errbuff);
    if(strlen(errbuff)) {
        printf("can't find %s\n%s\n", filename, errbuff);
        exit(1);
    }

    int packetID = 0;
    while(true) {
        pcap_pkthdr *packet_header; 
        ether_header *eth_header;
        time_t time_t_tmp;
        u_char *packet;
        int res = pcap_next_ex(handler, &packet_header, (const u_char **)&packet); // capture one packet one time
        if(res == 0) 
            continue;
        else if(res == -2) {
            char line[100];
            sprintf(line, "It is the last packet of %s", filename);
            printf("\n ▶ %s ◀ \n\n", line);
            // separateLine(line);
            break;
        }
        else if(res == -1) {
            printf("pcap_next_ex ERROR\n");
            exit(1);
        }

        time_t_tmp = packet_header->ts.tv_sec; // ts => time stamp , tv_sec => in second
        struct tm ts = *localtime(&time_t_tmp);
        char timeStamp[50];
        strftime(timeStamp, sizeof(timeStamp),"%a %Y-%m-%d %H:%M:%S", &ts); // store the localtime into str_time

        eth_header = (ether_header *)packet;
        
        char MAC_src[MaxMACLEN], MAC_des[MaxMACLEN];
        strcpy(MAC_src, MAC_ntoa(eth_header->ether_shost)); /* source ether addr	*/
        strcpy(MAC_des, MAC_ntoa(eth_header->ether_dhost)); /* destination eth addr	*/

        char line[100];
        sprintf(line, "PacketID: %d", ++packetID);
        separateLine(line);

        printf("┌───────────────────┬─────────────────────────┐\n");
        printf("│ PacketID          │                    %4d │\n", packetID);       
        printf("├───────────────────┼─────────────────────────┤\n");
        printf("│ Time Stamp        │ %s │\n", timeStamp);       
        printf("├───────────────────┼─────────────────────────┤\n");
        printf("│ Length            │                   %5d │\n", packet_header->len);       
        printf("├───────────────────┼─────────────────────────┤\n");
        printf("│ Capture Length    │                   %5d │\n", packet_header->caplen);       
        printf("└───────────────────┴─────────────────────────┘\n");

        printf("┌─────────────────────────┬───────────────────┐\n");
        printf("│ MAC Sourse address      │ %15s │\n", MAC_src);       
        printf("├─────────────────────────┼───────────────────┤\n");
        printf("│ MAC Destination address │ %15s │\n", MAC_des);       
        printf("└─────────────────────────┴───────────────────┘\n");

        unsigned short type = ntohs(eth_header->ether_type); // net to host short int
        /* IPv4 */
        if(type == ETHERTYPE_IP) {
            ip *ip_header = (ip *)(packet + ETHER_HDR_LEN);
            char ip_src[INET_ADDRSTRLEN];
            char ip_des[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ip_header->ip_src), ip_src, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &(ip_header->ip_dst), ip_des, INET_ADDRSTRLEN);

            printf("┌──────────────────┬────────┐\n");
            printf("│ Ethernet type    │   IPv4 │\n");       
            printf("└──────────────────┴────────┘\n");

            printf("┌────────────────────────┬─────────────────┐\n");
            printf("│ IP Sourse address      │ %15s │\n", ip_src);       
            printf("├────────────────────────┼─────────────────┤\n");
            printf("│ IP Destination address │ %15s │\n", ip_des);       
            printf("└────────────────────────┴─────────────────┘\n");

            TCP_or_UDP(ip_header, packet);
        /* IPv6 */
        } else if(type == ETHERTYPE_IPV6) {
            ip6_hdr *ip6_header = (ip6_hdr *)(packet + ETHER_HDR_LEN);
            printf("┌──────────────────┬────────┐\n");
            printf("│ Ethernet type    │   IPv6 │\n");       
            printf("└──────────────────┴────────┘\n"); 
            char ip_src[INET6_ADDRSTRLEN];
            char ip_des[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(ip6_header->ip6_src), ip_src, INET6_ADDRSTRLEN);
            inet_ntop(AF_INET6, &(ip6_header->ip6_dst), ip_des, INET6_ADDRSTRLEN);

            printf("┌────────────────────────┬─────────────────────────────────────────────┐\n");
            printf("│ IP Sourse address      │ %43s |\n", ip_src);                         
            printf("├────────────────────────┼─────────────────────────────────────────────┤\n");
            printf("│ IP Destination address │ %43s |\n", ip_des);       
            printf("└────────────────────────┴─────────────────────────────────────────────┘\n"); 

            TCP_or_UDP_for_IPv6(ip6_header, packet); 

        /* Others */
        } else if(type == ETHERTYPE_ARP) {
            printf("┌──────────────────┬────────┐\n");
            printf("│ Ethernet type    │    ARP │\n");       
            printf("└──────────────────┴────────┘\n");
        } else if(type == ETHERTYPE_PUP) {
            printf("┌──────────────────┬────────┐\n");
            printf("│ Ethernet type    │    PUP │\n");       
            printf("└──────────────────┴────────┘\n");  
        } else if(type == ETHERTYPE_IPX) {
            printf("┌──────────────────┬────────┐\n");
            printf("│ Ethernet type    │    IPX │\n");       
            printf("└──────────────────┴────────┘\n");  
        } else if(type == ETHERTYPE_VLAN) {
            printf("┌──────────────────┬────────┐\n");
            printf("│ Ethernet type    │   VLAN │\n");       
            printf("└──────────────────┴────────┘\n");  
        } else if(type == ETHERTYPE_SPRITE) {
            printf("┌──────────────────┬────────┐\n");
            printf("│ Ethernet type    │ SPRITE │\n");       
            printf("└──────────────────┴────────┘\n");  
        } else {
            printf("┌──────────────────┬────────────┐\n");
            printf("│ Ethernet type    │ No support │\n");       
            printf("└──────────────────┴────────────┘\n"); 
        }
    }

    return 0;
}

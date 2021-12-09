#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <netinet/in.h>

#if !defined(MACRO)
#define MACRO
#define PACKET_MAXLEN 65535
#define PICTURE_MAXLEN (int)1e7
#define LISTENQ 1024
#define SERVER_PORT 8000
#define SA struct sockaddr
#endif // MACRO

char TCP_head[] =   "HTTP/1.1 200 OK\r\n";
char TCP_head_send_txt[] =  "HTTP/1.1 200 OK\r\n"
					"Content-Type: image/txt\r\n";

char HTML1[] =		"\r\n"
					"<!DOCTYPE html>\r\n"
					"<html><head><title>409410014_Web_Server</title>\r\n"
					"</head>\r\n"
					"<body>\r\n";
char select_img[] =	"<form action=\"\" method=\"post\" enctype=\"multipart/form-data\">\r\n"
					"Please upload a image<br>\r\n"
					"<input type=\"file\" name=\"img\">\r\n"
					"<button type=\"submit\">Submit</button>\r\n"
					"</form>\r\n";
char text[] =		"<img src=\"/img.txt\" alt=\"No img\">\r\n";
char HTML2[] =		"</body></html>\r\n";

int listenfd;
int connfd;

u_char *buffer;
u_char *picture;

void yield_html_packet(u_char *packet){
    packet[0] = '\0';
    strcat(packet, TCP_head);
	strcat(packet, HTML1);
	strcat(packet, select_img);
	strcat(packet, text);
	strcat(packet, HTML2);
}

u_char *get_picture_start_ptr(u_char *packet){
    u_char *c = packet;
	c = strstr(c, "\r\n\r\n");
	c = c + 5;
	c = strstr(c, "\r\n\r\n");
	c = c + 4;
	return c;
}

int get_content_LEN (u_char *packet){
    u_char *line = strstr(packet, "Content-Length: ");
	u_char n[10];
	int idx = 0;
	while(*(++line) != '\n') {
		if(isdigit(*line)) n[idx++] = *line;
	}
	n[idx] = 0;
	return atoi(n);
}

void strcat_picture_packet(u_char *packet){
    packet[0] = '\0';
	strcat(packet, TCP_head_send_txt);
	strcat(packet, (u_char *)picture);
}

int print_and_check_packet_msg(u_char *packet){
    if(strncmp(buffer, "GET ", 4) == 0 && strstr(buffer, "GET /img.txt")) {
        printf("Browser request Image\n");
        return 1;
    }

    else if(strncmp(buffer, "GET ", 4) == 0 && strstr(buffer, "Accept: text/html")){
        printf("Browser request Index\n");
        return 2;
    }

    else if(strncmp(buffer, "POST ", 5) == 0 && strstr(buffer, "multipart/form-data")){
        printf("Browser send a file\n");
        return 3;
    }

    else {
        printf("Browser send a packet\n");
        return 4;
    }
}

int main(int argc,char **argv){
    struct sockaddr_in servaddr;
    u_char *packet;

    // allocate size 
    buffer = (u_char*) malloc((long long)1e10 * sizeof(u_char));
    picture = (u_char*) malloc((long long)1e10 * sizeof(u_char));
    packet = (u_char*) malloc((long long)1e7 * sizeof(u_char));

    // create listenfd
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd == -1){
        printf("socket error\n");
        exit(0);
    }

    // initialize sever address
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERVER_PORT);

    // check bind
    if(bind(listenfd, (SA*)&servaddr, sizeof(servaddr)) == -1){
        printf("Bind error\n");
		exit(1);
    }

    // check listen
    if(listen(listenfd, LISTENQ) == -1) {
		printf("Listen error\n");
		exit(1);
	}
    printf("Listening...\n");

    // to prevent zombie process
    signal(SIGCHLD, SIG_IGN);

    while(1){
        // await a connection on socket FD
        connfd = accept(listenfd, (SA*)NULL, NULL);
        if(connfd == -1){
            printf("Accept error\n");
            exit(1);
        }
        printf("Packet accepted\n");

        // fork process
        pid_t pid = fork();

        // check fork
        if(pid == -1) {
			printf("fork error\n");
			exit(1);
		}

        // pid == 0 -> ok!
        if(!pid){
            // check read
            if(read(connfd, buffer, PICTURE_MAXLEN) == -1) {
                printf("read error\n");
                exit(0);
            }

            printf("%s\n", buffer);
            packet[0] = '\0';

            int packet_kind = print_and_check_packet_msg(buffer);

            if(packet_kind == 1) {
                // read image in binary
                FILE *image = fopen("img.txt", "rb");
                // check file open
                if(image == NULL) {
                    printf("fopen error\n");
                    exit(1);
                }

                int imagefd = fileno(image);

                struct stat statbuffer;
                fstat(imagefd, &statbuffer);

                strcat(packet, TCP_head_send_txt);
                sprintf(buffer, "Content-Length: %d\r\n", (int)statbuffer.st_size);
                strcat(packet, buffer); 
                strcat(packet,"\r\n");

                // check write
                if(write(connfd, packet, strlen(packet)) == -1){
                    printf("write error\n");
                    exit(1);
                }

                sendfile(connfd, imagefd, 0, statbuffer.st_size);
                packet[0] = '\0';
            }
            else if(packet_kind == 2){
                yield_html_packet(packet);
                // check write
                if(write(connfd, packet, strlen(packet)) == -1) {
					printf("write error\n");
					exit(1);
				}
            }
            // upload file
            else if(packet_kind == 3){
                u_char *start_ptr = get_picture_start_ptr(buffer);
                int LEN = get_content_LEN(buffer); // LEN == image size
                FILE *fp = fopen("img.txt", "w");
                // check file write
                if(fp == NULL){
                    printf("fopen error\n");
                    exit(1);
                }
                printf("\n%d\n", LEN);
                fwrite(start_ptr, (sizeof(u_char) * LEN), 1, fp);
                yield_html_packet(packet);
                if(write(connfd, packet, strlen(packet)) == -1){
                    printf("write error\n");
                    exit(1);
                }
            }
            else{
                yield_html_packet(packet);
                // check write
                if(write(connfd, packet, strlen(packet)) == -1){
                    printf("write error\n");
                    exit(1);
                }
            }

            packet[0] = '\0';
            buffer[0] = '\0';
            // check close
            if(close(connfd) == -1){
                printf("close error\n");
                exit(1);
            }
            exit(0);
        }
        else {
            if(close(connfd) == -1){
                printf("close error\n");
                exit(1);
            }
        }

    }

    return 0;
}
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#define PORT 10000
#define BUF_SZ 1024

int sockfd = -1;
int user_id = -1;

int stdin_rd_status = 0;  // 0: normal, 1: w_invite_target,
                          // 2: w_invite_response, 3: in_game
int sock_rd_status = 0;   // 0: normal, 1: w_online_resp, 2: w_invite_resp,
                          // 3: w_accept_invite_resp, 4: w_end_game_resp

int last_invite_no = -1;
int game_partner_no = -1;

void send_server(char *buf) {
    if (write(sockfd, buf, strlen(buf)) < 0) {
        perror("Error writing to server");
    }
}

void show_menu() {
    printf("Enter a number:\n");
    printf("1) Online list\n");
    printf("2) Invite\n");
    printf("3) Logout\n");
}

void handle_game_update(char *buf);

void handle_ingame_stdin(char *buf);

char board[3][3] = {  // to display the actual game status
    {' ', ' ', ' '},
    {' ', ' ', ' '},
    {' ', ' ', ' '}};

int player = 0;

char numberBoard[3][3] = {  // to display positions to choose from
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'}};

#define IS_MY_TURN (player == (user_id > game_partner_no))

void welcome();
void printBoard(char board[][3]);
void check(char board[][3]);
void show_my_turn();

int main(int argc, char **argv) {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket()");
        exit(1);
    }

    struct sockaddr_in servaddr;

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);

    if (argc < 2) {
        servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    } else {
        servaddr.sin_addr.s_addr = inet_addr(argv[1]);
    }

    if (connect(sockfd, &servaddr, sizeof(servaddr)) < 0) {
        perror("connect()");
        exit(1);
    }
    printf("Server connected!\n");

    while (1) {
        printf("Enter user_id: ");
        scanf("%d", &user_id);

        char password[100];
        printf("Enter password: ");
        scanf("%s", password);

        char send_buf[BUF_SZ] = {0};
        sprintf(send_buf, "1 %d %s\n", user_id, password);
        send_server(send_buf);

        char read_buf[BUF_SZ] = {0};
        if (read(sockfd, read_buf, BUF_SZ - 1) < 0) {
            perror("Error reading from server");
            return 1;
        }

        int response_status;
        sscanf(read_buf, "%d", &response_status);
        if (response_status == 0) {
            printf("Welcome user %d\n", user_id);
            break;
        } else {
            printf("Error: %s\n", read_buf + 2);
        }
    }

    char read_buf[BUF_SZ] = {0};
    int read_count;

    fd_set rfds;
    fd_set errfds;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500;

    show_menu();

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(0, &rfds);
        FD_SET(sockfd, &rfds);

        int retval = select(sockfd + 1, &rfds, NULL, &errfds, &tv);

        if (FD_ISSET(0, &errfds)) {
            printf("error on stdin\n");
        } else if (FD_ISSET(sockfd, &errfds)) {
            printf("error on socket\n");
        }

        if (retval == -1)
            perror("select()");
        else if (retval) {
            char read_buf[BUF_SZ] = {0};

            int fd = FD_ISSET(0, &rfds) ? 0 : sockfd;

            int read_count = read(fd, read_buf, BUF_SZ - 1);
            if (read_count < 0) {
                perror("read()");
                continue;
            }

            if (read_count == 0) {
                exit(0);
            }

            if (fd == 0) {
                // stdin read
                if (stdin_rd_status == 0) {
                    // handling menu
                    int cmdnum = -1;
                    sscanf(read_buf, "%d", &cmdnum);

                    switch (cmdnum) {
                        case 1:
                            send_server("2\n");
                            sock_rd_status = 1;
                            break;
                        case 2:
                            printf("Enter the user_id you want to invite: ");
                            fflush(stdout);
                            stdin_rd_status = 1;
                            break;
                        case 3:
                            send_server("6\n");
                            exit(0);
                            break;
                        default:
                            break;
                    }
                } else if (stdin_rd_status == 1) {
                    // read invite target
                    int invite_target = -1;
                    sscanf(read_buf, "%d", &invite_target);

                    char buf[BUF_SZ] = {0};
                    sprintf(buf, "3 %d\n", invite_target);
                    send_server(buf);
                    sock_rd_status = 2;
                    stdin_rd_status = 0;
                    last_invite_no = invite_target;
                } else if (stdin_rd_status == 2) {
                    // wait accept/deny invite input
                    if (read_buf[0] == 'y') {
                        char buf[BUF_SZ] = {0};
                        sprintf(buf, "4 %d\n", last_invite_no);
                        send_server(buf);
                        game_partner_no = last_invite_no;
                    } else {
                        char buf[BUF_SZ] = {0};
                        sprintf(buf, "5 %d\n", last_invite_no);
                        send_server(buf);
                    }
                    last_invite_no = -1;
                    stdin_rd_status = 0;
                    sock_rd_status = 3;
                } else if (stdin_rd_status == 3) {
                    handle_ingame_stdin(read_buf);
                }
            } else {
                // socket read
                if (read_buf[0] == 's') {
                    // server msg
                    int serv_msg_id;
                    sscanf(read_buf, "s %d", &serv_msg_id);

                    switch (serv_msg_id) {
                        case 1: {
                            int src;
                            sscanf(read_buf, "s 1 %d", &src);

                            printf(
                                "User %d wants to invite you to play the "
                                "game\nAccpet? (y/n) ",
                                src);
                            fflush(stdout);

                            last_invite_no = src;
                            stdin_rd_status = 2;
                            break;
                        }
                        case 2:
                            printf("Sorry, your invite had beed rejected!\n");
                            last_invite_no = -1;
                            break;
                        case 3:
                            welcome();
                            if (game_partner_no == -1)
                                game_partner_no = last_invite_no;

                            if (IS_MY_TURN) {
                                printBoard(numberBoard);
                                show_my_turn();
                            }

                            check(board);

                            stdin_rd_status = 3;
                            break;
                        case 4:
                            handle_game_update(read_buf + 4);
                            break;
                        default:
                            printf("Serv_msg: %s\n", read_buf + 2);
                            break;
                    }
                } else if (sock_rd_status == 1) {
                    // waiting online list
                    int online_count;
                    int offset = 0;
                    int bread;

                    sscanf(read_buf + offset, "%d%n", &online_count, &bread);
                    offset += bread;

                    printf("%d user online:", online_count);

                    for (int i = 0; i < online_count; i++) {
                        int user_id;
                        sscanf(read_buf + offset, "%d%n", &user_id, &bread);
                        offset += bread;

                        printf(" %d", user_id);
                    }
                    printf("\n");
                    sock_rd_status = 0;
                } else if (sock_rd_status == 2) {
                    // waiting invite response
                    if (read_buf[0] == '0') {
                        printf("Invite sent!\n");
                    } else {
                        printf("Error: %s\n", read_buf + 2);
                    }
                    sock_rd_status = 0;
                } else if (sock_rd_status == 3) {
                    if (read_buf[0] == '1') {
                        printf("Error: %s\n", read_buf + 2);
                        game_partner_no = -1;
                    }
                    sock_rd_status = 0;
                } else if (sock_rd_status == 4) {
                    if (read_buf[0] == '1') {
                        printf("Error: %s\n", read_buf + 2);
                    } else if (read_buf[0] == '0') {
                        game_partner_no = -1;
                        stdin_rd_status = 0;
                        printf("\n");
                        show_menu();
                    }
                    sock_rd_status = 0;
                } else {
                    printf("Server: %s\n", read_buf);
                }
            }
        }
    }

    return 0;
}

void welcome() {
    printf("==================================\n");
    printf("[Welcome to the |TIC-TAC-TOE| !!!]\n");
    printf("==================================\n");
}

void printBoard(char board[][3]) {
    printf("\n =================\n");
    printf("[     |     |     ] \n");
    printf("[  %c  |  %c  |  %c  |\n", board[0][0], board[0][1], board[0][2]);
    printf("[=====|=====|=====]\n");
    printf("[     |     |     ]\n");
    printf("[  %c  |  %c  |  %c  |\n", board[1][0], board[1][1], board[1][2]);
    printf("[=====|=====|=====]\n");
    printf("[     |     |     ]\n");
    printf("[  %c  |  %c  |  %c  |\n", board[2][0], board[2][1], board[2][2]);
    printf("[=====|=====|=====]\n");
}

void check(char board[][3]) {
    int i,cnt = 0,draw = 0;
    char win = ' ';

    for(int i= 0;i< 3;i++){
        for(int j= 0;j< 3;j++){
            if(board[i][j] != ' ')
                cnt++;
        }
    }

    if (cnt == 9){
        printf("DRAW!\n");
        draw = 1;
    }

    // rows
    for (i = 0; i < 3; i++)
        if (board[i][0] == board[i][1] && board[i][0] == board[i][2] &&
            board[i][0] != ' ')
            win = board[i][0];
    // columns
    for (i = 0; i < 3; i++)
        if (board[0][i] == board[1][i] && board[0][i] == board[2][i] &&
            board[0][i] != ' ')
            win = board[0][i];
    // diagonals
    if (board[0][0] == board[1][1] && board[1][1] == board[2][2] &&
        board[1][1] != ' ')
        win = board[1][1];
    if (board[0][2] == board[1][1] && board[1][1] == board[2][0] &&
        board[1][1] != ' ')
        win = board[1][1];

    if (win == 'X') {
        printBoard(board);
        printf("Player 1 Wins\n");
    }

    if (win == 'O') {
        printBoard(board);
        printf("Player 2 Wins\n	");
    }

    if (win == 'X' || win == 'O' || draw == 1) {
        send_server("8\n");
        sock_rd_status = 4;
    }
}

void show_my_turn() {
    printf(
        "\nPlayer %d, Please enter the number of the position where "
        "you want to place your %c: ",
        player, (player == 1) ? 'X' : 'O');
    fflush(stdout);
}

void handle_game_update(char *buf) {
    int cmd = -1;
    sscanf(buf, "%d", &cmd);

    switch (cmd) {
        case 1: {
            int choice;
            sscanf(buf, "1 %d", &choice);

            int row, column;
            row = --choice / 3;
            column = choice % 3;

            board[row][column] = (player == 1) ? 'X' : 'O';

            player = (player == 0 ? 1 : 0);

            printBoard(board);

            if (IS_MY_TURN) {
                printBoard(numberBoard);
                show_my_turn();
            }

            check(board);

            break;
        }
        default:
            fprintf(stderr, "unknown cmd %d\n", cmd);
            break;
    }
}

void handle_ingame_stdin(char *buf) {
    if (IS_MY_TURN) {
        // is my turn

        int choice;
        sscanf(buf, "%d", &choice);

        int row, column;
        row = --choice / 3;
        column = choice % 3;

        if (choice < 0 || choice > 9 || board[row][column] > '9' ||
            board[row][column] == 'X' || board[row][column] == 'O') {
            printf("Invalid Input. Please Enter again.\n");
        } else {
            board[row][column] = (player == 1) ? 'X' : 'O';

            player = (player == 0 ? 1 : 0);

            {
                char send_buf[BUF_SZ] = {0};
                sprintf(send_buf, "7 1 %d\n", choice + 1);
                send_server(send_buf);
            }

            printBoard(board);

            check(board);
        }
    } else {
        // not my
    }
}

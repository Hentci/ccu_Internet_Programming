#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PORT 10000
#define BUF_SZ 1024

void handle_error() {
    fprintf(stderr, "Error: %s\n", strerror(errno));
    return;
}

#define MAX_USER 100

int user_connfd_table[MAX_USER];  // -1 or fd_num
char *user_passwords[MAX_USER];   // string or NULL
int user_peer[MAX_USER];          // -1 or user_id
int user_peer_status[MAX_USER];   // 1 for invite_sender, 2 for gaming

void *handle_client(void *data);

int main() {
    pthread_t pt;

    for (int i = 0; i < MAX_USER; i++) {
        user_connfd_table[i] = -1;
        user_passwords[i] = NULL;
        user_peer[i] = -1;
    }

    user_passwords[0] = "0";
    user_passwords[1] = "1";
    user_passwords[2] = "2";

    int connfd;
    struct sockaddr_in servaddr;

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int reuseAddrOption = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuseAddrOption,
               sizeof(reuseAddrOption));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        fprintf(stderr, "Unable to bind: ");
        handle_error();
        exit(1);
    }

    // listen
    if (listen(listenfd, 10) == -1) {
        fprintf(stderr, "Unable to listen: ");
        handle_error();
        exit(1);
    }

    while (1) {
        int connfd;

        if ((connfd = accept(listenfd, (struct sockaddr *)NULL, 0)) < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "Unable to accept: ");
            handle_error();
            continue;
        }

        pthread_create(&pt, NULL, handle_client, &connfd);
    }

    return 0;
}

void send_client(int connfd, char *buf) {
    if (write(connfd, buf, strlen(buf)) < 0) {
        fprintf(stderr, "Error writing to %d: ", connfd);
        handle_error();
    }
}

#define CHECK_USER_INVALID(user_id)            \
    ((user_id) < 0 || (user_id) >= MAX_USER || \
     user_passwords[(user_id)] == NULL)

/**
 * Login
 * format: 1 <user_id> <password>
 * response: ok: 0, error: 1 <msg>
 */
int login(int connfd, char *buf) {
    int user_id = -1;
    char password[100] = {0};

    sscanf(buf, "1 %d %s", &user_id, password);

    if (CHECK_USER_INVALID(user_id)) {
        send_client(connfd, "1 no_such_user\n");
        return -1;
    }

    if (strcmp(user_passwords[user_id], password) == 0) {
        send_client(connfd, "0\n");
        user_connfd_table[user_id] = connfd;
        return user_id;
    } else {
        send_client(connfd, "1 wrong_password\n");
    }

    return -1;
}

/**
 * Online
 * format: 2
 * response: <online_count> <online_user_id_vector(space between user_id)>
 */
void online(int connfd) {
    char online_buf[BUF_SZ] = {0};

    int online_count = 0;
    for (int i = 0; i < MAX_USER; i++) {
        if (user_connfd_table[i] != -1) {
            sprintf(&online_buf[strlen(online_buf)], "%d ", i);
            online_count++;
        }
    }

    char all_buf[BUF_SZ] = {0};
    sprintf(all_buf, "%d %s\n", online_count, online_buf);
    send_client(connfd, all_buf);
    printf("Online info sent!\n");
}

/**
 * Invite
 * format: 3 <user_id>
 * response: ok: 0, error: 1 <msg>
 */
void invite(int connfd, int my_user_id, char *buf) {
    int target_user_id;
    sscanf(buf, "3 %d", &target_user_id);

    if (my_user_id == -1) {
        send_client(connfd, "1 login_first\n");
        return;
    }

    if (my_user_id == target_user_id) {
        send_client(connfd, "1 cannot_self_invite\n");
        return;
    }

    if (CHECK_USER_INVALID(target_user_id)) {
        send_client(connfd, "1 no_such_user\n");
        return;
    }

    if (user_connfd_table[target_user_id] == -1) {
        send_client(connfd, "1 user_not_online\n");
        return;
    }

    if (user_peer[target_user_id] != -1) {
        send_client(connfd, "1 user_not_available\n");
        return;
    }

    user_peer[my_user_id] = target_user_id;
    user_peer_status[my_user_id] = 1;

    char send_buf[BUF_SZ] = {0};
    sprintf(send_buf, "s 1 %d\n", my_user_id);
    send_client(user_connfd_table[target_user_id], send_buf);

    send_client(connfd, "0\n");
}

/**
 * Accept Invite
 * format: 4 <user_id>
 * response: ok: 0, error: 1 <msg>
 */
void accpet_invite(int connfd, int my_user_id, char *buf) {
    int target_user_id;
    sscanf(buf, "4 %d", &target_user_id);

    if (my_user_id == -1) {
        send_client(connfd, "1 login_first\n");
        return;
    }

    if (CHECK_USER_INVALID(target_user_id)) {
        send_client(connfd, "1 no_such_user\n");
        return;
    }

    if (user_peer_status[target_user_id] != 1 ||
        user_peer[target_user_id] != my_user_id) {
        send_client(connfd, "1 invalid\n");
        return;
    }

    user_peer[my_user_id] = target_user_id;
    user_peer_status[my_user_id] = 2;
    user_peer_status[target_user_id] = 2;

    send_client(connfd, "0\n");
    send_client(connfd, "s 3\n");
    send_client(user_connfd_table[target_user_id], "s 3\n");
}

/**
 * Deny Invite
 * format: 5 <user_id>
 * response: ok: 0, error: 1 <msg>
 */
void deny_invite(int connfd, int my_user_id, char *buf) {
    int target_user_id;
    sscanf(buf, "5 %d", &target_user_id);

    if (my_user_id == -1) {
        send_client(connfd, "1 login_first\n");
        return;
    }

    if (CHECK_USER_INVALID(target_user_id)) {
        send_client(connfd, "1 no_such_user\n");
        return;
    }

    if (user_peer_status[target_user_id] != 1 ||
        user_peer[target_user_id] != my_user_id) {
        send_client(connfd, "1 invalid\n");
        return;
    }

    user_peer[target_user_id] = -1;
    user_peer_status[target_user_id] = 0;

    send_client(connfd, "0\n");
    send_client(user_connfd_table[target_user_id], "s 2\n");
}

/**
 * Game Update
 * format: 7 <update_msg>
 * response: ok: 0, error: 1 <msg>
 */
void game_update(int connfd, int my_user_id, char *buf) {
    if (my_user_id == -1) {
        send_client(connfd, "1 login_first\n");
        return;
    }

    if (user_peer[my_user_id] == -1 || user_peer_status[my_user_id] != 2) {
        send_client(connfd, "1 not_in_game\n");
        return;
    }

    char send_buf[BUF_SZ] = {0};

    if (buf[strlen(buf) - 1] == '\n') buf[strlen(buf) - 1] = '\0';
    sprintf(send_buf, "s 4 %s\n", buf + 2);
    send_client(user_connfd_table[user_peer[my_user_id]], send_buf);
}

/**
 * Game End
 * format: 8
 * response: ok: 0, error: 1 <msg>
 */
void game_end(int connfd, int my_user_id) {
    if (my_user_id == -1) {
        send_client(connfd, "1 login_first\n");
        return;
    }

    if (user_peer[my_user_id] == -1 || user_peer_status[my_user_id] != 2) {
        send_client(connfd, "1 not_in_game\n");
        return;
    }

    user_peer_status[my_user_id] = 0;
    user_peer[my_user_id] = -1;

    send_client(connfd, "0\n");
}

void *handle_client(void *data) {
    int connfd = *(int *)data;

    printf("New connection on %d\n", connfd);

    char read_buf[BUF_SZ] = {0};

    int curr_user_id = -1;

    int read_count;
    while ((read_count = read(connfd, read_buf, BUF_SZ - 1)) > 0) {
        printf("%s\n", read_buf);

        int cmd_num = -1;
        sscanf(read_buf, "%d", &cmd_num);

        switch (cmd_num) {
            case 1:
                curr_user_id = login(connfd, read_buf);
                break;
            case 2:
                online(connfd);
                break;
            case 3:
                invite(connfd, curr_user_id, read_buf);
                break;
            case 4:
                accpet_invite(connfd, curr_user_id, read_buf);
                break;
            case 5:
                deny_invite(connfd, curr_user_id, read_buf);
                break;
            case 6:
                goto Exit;
                break;
            case 7:
                game_update(connfd, curr_user_id, read_buf);
                break;
            case 8:
                game_end(connfd, curr_user_id);
                break;
            default:
                send_client(connfd, "1 no_such_cmd\n");
                break;
        }

        memset(read_buf, 0, sizeof(read_buf));
    }

Exit:

    if (read_count < 0) {
        fprintf(stderr, "Read client error: ");

        handle_error();
    }

    if (curr_user_id != -1) {
        user_connfd_table[curr_user_id] = -1;
        user_peer[curr_user_id] = -1;
        user_peer_status[curr_user_id] = 0;
    }

    printf("client exit %d\n", connfd);

    pthread_exit(0);
}

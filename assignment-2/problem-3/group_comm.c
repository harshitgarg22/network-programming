#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "constants.h"

typedef struct {
    char *grp_name;
    char mcast_group_addr[MAX_ADDR_LEN];
    int send_sockfd;
    struct sockaddr_in send_addr;
    int recv_sockfd;
    struct sockaddr_in recv_addr;
    bool is_joined;
} comm_grp;


/**
 * Fields describing all the groups that the user is aware of. 
 * These should ideally be the same for all peers on the LAN
 * because any group creation event is broadcasted to all peers
 */
int grp_count;                          /* Number of grps in the database*/
comm_grp grp_db[MAX_GRPS_ALLOWED];      /* list of all comm_grp structs */

int return_grp_key(char *grp_name) {
    if (!grp_name) return -1;
    for (int i = 0; i < grp_count; i++) {
        if (strcmp(grp_name, grp_db[i].grp_name) == 0) {
            return i;
        }
    } 
    return -1;
}

int create_grp(char *grp_name) {
    comm_grp *new_grp;
}

int join_grp(char *grp_name) {
    for (int i = 0; i < grp_count; i++) {
        if (strcmp(grp_name, grp_db[i].grp_name) == 0) {
            // match found
            if (grp_db[i].is_joined) {
                printf("You're already a member of this group\n");
                return 0;
            }
            else {
                // join new grp
                struct ip_mreq mreq;
                mreq.imr_interface.s_addr = inet_addr(grp_db[i].mcast_group_addr);
                mreq.imr_interface.s_addr = htonl(INADDR_ANY);  // allow kernel to choose which interface to use for the mcast grp

                int sockopt;
                // add this host as a member on the receiving socket for the group
                if ((sockopt = setsockopt(grp_db[i].recv_sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))) == -1) {
                    printf("Could not join group **%s** due to error\n", grp_name);
                    exit(EXIT_FAILURE);
                }

                grp_db[i].is_joined = true;
                printf("You joined group **%s**. Welcome!\n", grp_name);
                return 1;
            }
        }
    }
    printf("No group found with this name\n");
    return -1;
}

int leave_grp(char *grp_name) {
    int num_grps = sizeof(grp_db) / sizeof(comm_grp);
    for (int i = 0; i < num_grps; i++) {
        if (strcmp(grp_name, grp_db[i].grp_name) == 0) {
            if (!grp_db[i].is_joined) {
                printf("Can't leave a grp you're not a member of, now, can you?\n");
                return 0;
            }
            else {
                struct ip_mreq mreq;
                mreq.imr_interface.s_addr = inet_addr(grp_db[i].mcast_group_addr);
                mreq.imr_interface.s_addr = htonl(INADDR_ANY);

                int sockopt;
                if ((sockopt = setsockopt(grp_db[i].recv_sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq))) == -1) {
                    printf("Could not leave group **%s** due to error\n", grp_name);
                    exit(EXIT_FAILURE);
                }

                grp_db[i].is_joined = false;
                printf("You're no longer a member of group **%s**\n", grp_name);
                return 1;
            }
        }
    }
    printf("No group found with this name\n");
    return -1;
}

int create_poll(char *grp_name) {
    int sendfd, recvfd;
    struct sockaddr_in sa, ra;
    int index = return_grp_key(grp_name);
    if (index == -1) {
        printf("No such grp exists\n");
        return -1;
    }
    if (!grp_db[index].is_joined) {
        printf("You are not a member of this grp\n");
        printf("Could not create poll\n");
        return -1;
    }
    else {
        sendfd = grp_db[index].send_sockfd;
        recvfd = grp_db[index].recv_sockfd;
        sa = grp_db[index].send_addr;
        ra = grp_db[index].recv_addr;
    }

    char poll_question[LARGE_STR_LEN];
    fprintf(stderr, "Enter a yes/no question for the poll: ");
    fgets(poll_question, SMALL_STR_LEN, stdin);
    poll_question[strlen(poll_question)-1] = 0;

    printf("You created a new poll!\n");
    printf("Sending poll info to all members of grp <%s>\n", grp_name);

    pid_t pid = fork();
    if (pid == 0) {
        printf("Waiting for responses...\n");
        int yes = 0, no = 0, n;
        char recvBuff[SMALL_STR_LEN];
        for (;;) {
            n = recvfrom(recvfd, recvBuff, SMALL_STR_LEN, 0, (struct sockaddr*)&ra, sizeof(ra));
            recvBuff[n] = 0;
            if (n < 0) {
                printf("Could not receive response from peer\n");
                continue;
            }
            if (strcmp(recvBuff, "yes") == 0) yes++;
            else if (strcmp(recvBuff, "no") == 0) no++;
            else printf("Invalid response from peer\n");
        }

        printf("Poll complete!\n");
        printf("These are the results: YES %d | NO %d\n\n", yes, no);
        exit(EXIT_SUCCESS);
    }

    if (sendto(sendfd, poll_question, strlen(poll_question), 0, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        perror("Error sending poll to grp due to sendto()\n");
        return -1;
    }
}

int main(int argc, const char *argv[]) {
    printf("List of avaiable commands:\n");
    printf("-> create-grp <grp_name>\n");
    printf("-> join <grp_name>\n");
    printf("-> leave <grp_name>\n");
    printf("-> search-grp <keyword>\n");
    printf("-> get-file <filename>\n");
    printf("-> start-poll <grp_name>\n");

    printf("\n");
    while (true) {
        printf("\n>> ");
        char *cmd_buff = (char*)malloc(sizeof(char) * MAX_CMD_LEN);
        size_t cmd_len = MAX_CMD_LEN + 1;
        ssize_t cmd_inp_len = getline(&cmd_buff, &cmd_len, stdin);
        if (cmd_inp_len <= 0 ||  (cmd_inp_len == 1 && cmd_buff[0] == '\n')) {
            continue;
        }
        cmd_buff[cmd_inp_len-1] = 0;

        char *tokens[2];
        char *dup_cmd = strdup(cmd_buff);
        char *token = strtok(dup_cmd, __SPACE__);
        int i = 0;
        while (token) {
            if (token && i == 2) {
                fprintf(stderr, "More tokens than expected. Exiting...\n");
                exit(EXIT_FAILURE);
            }
            tokens[i++] = strdup(token);
        }

        if (strcmp(tokens[0], "create-grp") == 0) {
            create_grp(tokens[1]);
        }
        else if (strcmp(tokens[0], "join") == 0) {
            join_grp(tokens[1]);
        }
        else if (strcmp(tokens[0], "leave") == 0) {
            leave_grp(tokens[1]);
        }
        else if (strcmp(tokens[0], "search-grp") == 0) {
            search_for_grp(tokens[1]);
        }
        else if (strcmp(tokens[0], "get-file") == 0) {
            download_file(tokens[1]);
        }
        else if (strcmp(tokens[0], "start-poll") == 0) {
            create_poll(tokens[1]);
        }
    }
}
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <setjmp.h>

#include "constants.h"

/**
 * All message types that a peer can send/receive 
 * Responses to any message depend on this field 
 */
enum mtype { 
    FILE_ADVERTISE,
    FILE_ADVERTISE_FWD,
    FILE_REQUEST,
    FILE_REQUEST_FWD,
    POLL
};

/**
 * Basic message that is used for any multicast communication 
 * between members of a group
 */
typedef struct message {
    enum mtype msg_type;
    void *msg_buff;
} peer_msg;

/**
 * Data describing all the groups that the user is aware of. 
 * These should ideally be the same for all peers on the LAN
 * because any group creation event is broadcasted to all peers
 */
typedef struct {
    char *grp_name;
    char mcast_group_addr[MAX_ADDR_LEN];
    int send_sockfd;
    struct sockaddr_in send_addr;
    int recv_sockfd;
    struct sockaddr_in recv_addr;
    bool is_joined;
} comm_grp;

/* ---------------- Properties of a peer --------------------------- */

int grp_count;                          /* Number of grps in the database*/
comm_grp grp_db[MAX_GRPS_ALLOWED];      /* List of all comm_grp structs */
int bcastfd;                            /* Socket to send broadcastcast message to all members on LAN*/
struct sockaddr_in bcastAddr;           /* sockaddr struct to send broadcastcast message to all members on LAN */
char *files_available[MAX_FILES];       /* List of filenames available with this peer */

/* ----------------------------------------------------------------- */

/**
 * general error handling
 */
void handle_index_err(int i, char *grp_name) {
    if (i == -1) {
        fprintf(stderr,"Could not find group <%s> on this LAN\n", grp_name);
        exit(EXIT_FAILURE);
    }
}

/**
 * Utility functin to retrieve the index of grp_name
 * from the grp_db array 
 */
int get_grp_key(char *grp_name) {
    if (!grp_name) return -1;
    for (int i = 0; i < grp_count; i++) {
        if (strcmp(grp_name, grp_db[i].grp_name) == 0) {
            return i;
        }
    } 
    return -1;
}

/**
 * Creates a new group on the LAN and broadcasts the created
 * group's information to all peers on the LAN
 */ 
int create_grp(char *grp_name, char *mcast_ip, char *grp_port) {
    grp_db[grp_count].grp_name = strdup(grp_name);
    strcpy(grp_db[grp_count].mcast_group_addr, mcast_ip);

    struct sockaddr_in rcvAddr, sendAddr;
    int len = sizeof(rcvAddr);
    memset(&rcvAddr, 0, len);
    rcvAddr.sin_family = AF_INET;
    rcvAddr.sin_port = htons(atoi(grp_port));
    rcvAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    sendAddr = rcvAddr;
    sendAddr.sin_addr.s_addr = inet_addr(mcast_ip);

    int sendfd;
    if ((sendfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "Error creating send socket\n");
        return -1;
    }
    int useSame = 1;
    if (setsockopt(sendfd, SOL_SOCKET, SO_REUSEADDR, &useSame, sizeof(useSame)) < 0) {
        fprintf(stderr, "Error creating send socket due to setsockopt()\n");
        return -1;
    }

    int recvfd;
    if ((recvfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "Error creating recv socket\n");
        return -1;
    }
    if (bind(recvfd, (struct sockaddr*)&rcvAddr, len) < 0) {
        fprintf(stderr, "Error binding socket\n");
        return -1;
    }

    if (setsockopt(recvfd, IPPROTO_IP, IP_MULTICAST_LOOP, &useSame, sizeof(useSame)) < 0) {
        fprintf(stderr, "Error creating recv socket due to setsockopt()\n");
        return -1;
    }

    /* add this struct to the db */
    grp_db[grp_count].send_sockfd = sendfd;
    grp_db[grp_count].recv_sockfd = recvfd;
    grp_db[grp_count].send_addr = sendAddr;
    grp_db[grp_count].recv_addr = rcvAddr;

    comm_grp newgrp = grp_db[grp_count];
    int sent = sendto(bcastfd, &newgrp, sizeof(newgrp), 0, (struct sockaddr*)&bcastAddr, sizeof(bcastAddr));
    if (sent < 0) {
        fprintf(stderr, "Could not send new grp as broadcast\n");
        return -1;
    }
    grp_count += 1;
    return 0;
}

/**
 * Send the struct pointed to by 'buff' to all members of the
 * group 'grp_name'
 */
int notify_grp(char *grp_name, peer_msg msg) {
    int index = get_grp_key(grp_name);
    handle_index_err(index, grp_name);

    if (msg.msg_type == POLL) {
        if (!grp_db[index].is_joined) {
            printf("You are not a member of this grp\n");
            printf("Could not create poll\n");
            return -1;
        }
    }

    int sockfd = grp_db[index].send_sockfd; 
    struct sockaddr_in addr = grp_db[index].send_addr;

    if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return -1;
    }
    return 0;
}

int search_for_grp(char *grp_name) {
    int index = get_grp_key(grp_name);
    handle_index_err(index, grp_name);

    int port = ntohs(grp_db[index].send_addr.sin_port);
    printf("Found grp called <%s>\n", grp_name);
    printf("Multicast IP -> %s\n", grp_db[index].mcast_group_addr);
    printf("Multicast Grp Port -> %d\n", port);

    return 0;
}

int join_grp(char *grp_name) {
    int index = get_grp_key(grp_name);
    handle_index_err(index, grp_name);
    
    if (grp_db[index].is_joined) {
        printf("You're already a member of this group\n");
        return 0;
    }
    else {
        // join new grp
        struct ip_mreq mreq;
        mreq.imr_interface.s_addr = inet_addr(grp_db[index].mcast_group_addr);
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);  // allow kernel to choose which interface to use for the mcast grp

        // add this host as a member on the receiving socket for the group
        if (setsockopt(grp_db[index].recv_sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1) {
            printf("Could not join group <%s> due to error\n", grp_name);
            exit(EXIT_FAILURE);
        }
        
        grp_db[index].is_joined = true;
        printf("You joined group <%s>. Welcome!\n", grp_name);
        return 0;
    }
}

int leave_grp(char *grp_name) {
    int index = get_grp_key(grp_name);
    handle_index_err(index, grp_name);
    
    if (!grp_db[index].is_joined) {
        printf("You were never a member of this group\n");
        return 0;
    }
    else {
        // join new grp
        struct ip_mreq mreq;
        mreq.imr_interface.s_addr = inet_addr(grp_db[index].mcast_group_addr);
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);  // allow kernel to choose which interface to use for the mcast grp

        // add this host as a member on the receiving socket for the group
        if (setsockopt(grp_db[index].recv_sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) == -1) {
            printf("Could not leave group <%s> due to error\n", grp_name);
            exit(EXIT_FAILURE);
        }
        
        grp_db[index].is_joined = false;
        printf("You left group <%s>. Goodbye!\n", grp_name);
        return 0;
    }
}

int create_poll(char *grp_name) {

    char poll_question[LARGE_STR_LEN];
    fprintf(stderr, "Enter a yes/no question for the poll: ");
    fgets(poll_question, SMALL_STR_LEN, stdin);
    poll_question[strlen(poll_question)-1] = 0;

    printf("You created a new poll!\n");
    printf("Sending poll info to all members of grp <%s>\n", grp_name);

    peer_msg msg;
    notify_grp(grp_name, )
    if (sendto(sendfd, poll_question, strlen(poll_question), 0, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        fprintf(stderr, "Error sending poll to grp due to sendto()\n");
        return -1;
    }
    // pid_t pid = fork();
    // if (pid == 0) {
    //     printf("Waiting for responses...\n");
    //     int yes = 0, no = 0, n;
    //     char recvBuff[SMALL_STR_LEN];
    //     for ( ; ; ) {
    //         n = recvfrom(recvfd, recvBuff, SMALL_STR_LEN, 0, (struct sockaddr*)&ra, sizeof(ra));
    //         recvBuff[n] = 0;
    //         if (n < 0) {
    //             printf("Could not receive response from peer\n");
    //             continue;
    //         }
    //         if (strcmp(recvBuff, "yes") == 0) yes++;
    //         else if (strcmp(recvBuff, "no") == 0) no++;
    //         else printf("Invalid response from peer\n");
    //     }

    //     printf("Poll complete!\n");
    //     printf("These are the results: YES %d | NO %d\n\n", yes, no);
    //     exit(EXIT_SUCCESS);
    // }
}

int main(int argc, const char *argv[]) {

    bcastfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (bcastfd < 0) {
        printf("Could not create broadcast socket\n");
        exit(EXIT_FAILURE);
    }

    bcastAddr.sin_family = AF_INET;
    bcastAddr.sin_port = htons(BCAST_PORT);
    bcastAddr.sin_addr.s_addr = inet_addr("255.255.255.255");

    int bcastEnable = 1;
    if (setsockopt(bcastfd, SOL_SOCKET, SO_BROADCAST, &bcastEnable, sizeof(bcastEnable)) < 0) {
        fprintf(stderr, "Could not set broadcast option\n");
        exit(EXIT_FAILURE);
    }

    printf("List of avaiable commands:\n");
    printf("-> create-grp <grp_name> <mcast IP> <port>\n");
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

        char *tokens[4];
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
            create_grp(tokens[1], tokens[2], tokens[3]);
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
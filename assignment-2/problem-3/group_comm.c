
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
    FILE_REQUEST_RESPONSE,
    POLL,
    POLL_RESPONSE,
    DWNLD_FILE,
};

/**
 * Basic message that is used for any communication 
 * between members of a group
 */
typedef struct message {
    enum mtype msg_type;
    char msg_buff[LARGE_STR_LEN];
    char sender_ip[IP_ADDR_LEN];
    int flag;
} peer_msg;

/**
 * Data describing all the groups that the user is aware of. 
 * These should ideally be the same for all peers on the LAN
 * because any group creation event is broadcasted to all peers
 */
typedef struct {
    char *grp_name;
    char mcast_group_addr[IP_ADDR_LEN];
    int send_sockfd;
    struct sockaddr_in send_addr;
    int recv_sockfd;
    struct sockaddr_in recv_addr;
    bool is_joined;
} comm_grp;

/* ---------------- Properties of a peer ----------------- */

char unicast_ip[IP_ADDR_LEN];                    /* Unicast IP for this peer */
int GRP_COUNT;                          /* Number of grps in the database */
comm_grp grp_db[MAX_GRPS_ALLOWED];      /* List of all comm_grp structs */
int AVAILABLE_FILE_COUNT;               /* Number of files that the peer has locally */
char *files_available[MAX_FILES];       /* List of filenames available with this peer */
int maxfd;
fd_set readset, global_readset;
sig_atomic_t to_poll = 1;

int bcastfd;
struct sockaddr_in bcast_addr;
int ucast_recvfd;
struct sockaddr_in ucast_recv_addr;
/* ------------------------------------------------------- */

void poll_timeout_handler() {
    to_poll = 1;
}

/**
 * general error handling
 */
void err_exit(char *call) {
    fprintf(stderr, "\nError in *%s* call\n",call);
    perror(call);
}

/**
 * Utility functin to retrieve the index of grp_name
 * from the grp_db array 
 */
int get_grp_key(char *grp_name) {
    int index = -1;
    if (!grp_name) {
        fprintf(stderr, "Invalid grp name received in get_grp_key\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < GRP_COUNT; i++) {
        if (strcmp(grp_name, grp_db[i].grp_name) == 0) {
            index = i;
            break;
        }
    } 
    if (index == -1) {
        fprintf(stderr,"Could not find group <%s> on this LAN\nExiting...", grp_name);
        exit(EXIT_FAILURE);
    }
    return index;
}

/**
 * Create a new group on the LAN and broadcast the created
 * group's information to all peers on the LAN
 */ 
int create_grp(char *grp_name, char *mcast_ip, char *grp_port) {
    grp_db[GRP_COUNT].grp_name = strdup(grp_name);
    strcpy(grp_db[GRP_COUNT].mcast_group_addr, mcast_ip);

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
        err_exit("socket()");
        return -1;
    }
    int useSame = 1;
    if (setsockopt(sendfd, IPPROTO_IP, IP_MULTICAST_LOOP, &useSame, sizeof(useSame)) < 0) {
        err_exit("setsockopt()");
        return -1;
    }

    int recvfd;
    if ((recvfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        err_exit("socket()");
        return -1;
    }
    FD_SET(recvfd, &global_readset);

    if (bind(recvfd, (__SA__*)&rcvAddr, __ADDR_LEN__) < 0) {
        err_exit("bind()");
        return -1;
    }

    if (setsockopt(recvfd, SOL_SOCKET, SO_REUSEADDR, &useSame, sizeof(useSame)) < 0) {
        err_exit("setsockopt()");
        return -1;
    }

    /* add this struct to the db */
    grp_db[GRP_COUNT].send_sockfd = sendfd;
    grp_db[GRP_COUNT].recv_sockfd = recvfd;
    grp_db[GRP_COUNT].send_addr = sendAddr;
    grp_db[GRP_COUNT].recv_addr = rcvAddr;

    comm_grp newgrp = grp_db[GRP_COUNT];
    int sent = sendto(bcastfd, &newgrp, sizeof(newgrp), 0, (__SA__*)&bcast_addr, __ADDR_LEN__);
    if (sent < 0) {
        err_exit("sendto()");
        return -1;
    }
    GRP_COUNT += 1;
    return 0;
}

/**
 * Send the struct pointed to by 'buff' to all members of the
 * group 'grp_name'
 */
int notify_grp(char *grp_name, peer_msg msg) {
    int index = get_grp_key(grp_name);

    if (msg.msg_type == POLL) {
        if (!grp_db[index].is_joined) {
            printf("You are not a member of this grp\n");
            printf("Could not create poll\n");
            return -1;
        }
    }

    int sockfd = grp_db[index].send_sockfd; 
    struct sockaddr_in addr = grp_db[index].send_addr;

    strcpy(msg.sender_ip, unicast_ip);
    if (sendto(sockfd, &msg, sizeof(msg), 0, (__SA__*)&addr, __ADDR_LEN__) < 0) {
        err_exit("sendto()");
        return -1;
    }
    return 0;
}

/**
 * Search for GRP_NAME through all the groups known by this peer
 * If found, print its multicast IP and port.
 */ 
int search_for_grp(char *grp_name) {
    int index = get_grp_key(grp_name);

    int port = ntohs(grp_db[index].send_addr.sin_port);
    printf("Found grp called <%s>\n", grp_name);
    printf("Multicast IP -> %s\n", grp_db[index].mcast_group_addr);
    printf("Multicast Grp Port -> %d\n", port);

    return 0;
}

/**
 * Join the multicast group GRP_NAME
 */
int join_grp(char *grp_name) {
    int index = get_grp_key(grp_name);
    
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
            err_exit("setsockopt()");
            exit(EXIT_FAILURE);
        }
        
        grp_db[index].is_joined = true;
        if (grp_db[index].recv_sockfd > maxfd) {
            maxfd = grp_db[index].recv_sockfd;
        }
        printf("You joined group <%s>. Welcome!\n", grp_name);
        return 0;
    }
}

/**
 * Leave the multicast group GRP_NAME
 */
int leave_grp(char *grp_name) {
    int index = get_grp_key(grp_name);
    
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

/**
 * Create a new poll for GRP_NAME and send the poll request message
 * to all grp members
 */
int create_poll(char *grp_name) {
    char poll_question[LARGE_STR_LEN];
    fprintf(stderr, "Enter a yes/no question for the poll: ");
    fgets(poll_question, SMALL_STR_LEN, stdin);
    poll_question[strlen(poll_question)-1] = 0;

    printf("You created a new poll!\n");
    printf("Sending poll info to all members of grp <%s>\n", grp_name);

    peer_msg msg;
    strcpy(msg.msg_buff,poll_question);
    msg.msg_type = POLL;
    notify_grp(grp_name, msg);

    int index = get_grp_key(grp_name);
    int recvfd = grp_db[index].recv_sockfd;
    struct sockaddr_in recv_addr = grp_db[index].recv_addr;
    int len = __ADDR_LEN__;
    
    peer_msg recvmsg;
    if (fork() == 0) {
        int yes = 0, no = 0;    
        alarm(60);
        while (to_poll) {
            int n = recvfrom(recvfd, &recvmsg, SMALL_STR_LEN, 0, (__SA__*)&recv_addr, &len);
            if (n < 0) {
                err_exit("recvfrom()");
                continue;
            }
            if (msg.msg_type != POLL_RESPONSE)
                continue;
            if (strcmp(msg.msg_buff, "yes") == 0) yes++;
            else if (strcmp(msg.msg_buff, "no") == 0) no++;
            else {
                printf("Invalid response from peer\n");
                continue;
            }
        }
        to_poll = 1;
        printf("Poll complete!\n");
        printf("These are the results: YES %d | NO %d\n\n", yes, no);
        exit(EXIT_SUCCESS);
    }
    
}

/**
 * Advertise the list of files with this peer to each group
 * that it is a member of. This advertisement is sent every
 * 1 minute in the main process.
 */
int advertise_file_list() {
    /* prepare buffer containing filenames */
    peer_msg msg;
    msg.msg_type = FILE_ADVERTISE;
    char send_buff[LARGE_STR_LEN];

    for (int i = 0; i < AVAILABLE_FILE_COUNT; i++) {
        strcat(send_buff, files_available[i]);
        strcat(send_buff, __SPACE__);
    }
    strcpy(msg.msg_buff,send_buff);

    /* advertise file list to members */
    for (int i = 0; i < GRP_COUNT; i++) {
        if (grp_db[i].is_joined) {
            int n = notify_grp(grp_db[i].grp_name, msg);
            if (n == -1) {
                printf("Could not send file list\n");
                return -1;
            }
        }
    }
    return 0;
}

/**
 * Download filename from peer with this IP
 */ 
int download_file(peer_msg msg, char *filename) {
    int sendfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendfd < 0) {
        err_exit("socket()");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in send_addr;
    memset(&send_addr, 0, __ADDR_LEN__);
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = htons(UCAST_PORT);
    send_addr.sin_addr.s_addr = inet_addr(msg.sender_ip);

    peer_msg request_msg;
    request_msg.msg_type = DWNLD_FILE;
    int sent = sendto(sendfd, &request_msg, sizeof(peer_msg), 0, (__SA__*)&send_addr, __ADDR_LEN__);

    bool downloaded = false;
    FILE *fptr = fopen(filename, "wb");
    char dwnld_chunk[SMALL_STR_LEN];
    int len = __ADDR_LEN__;
    while (!downloaded) {
        int nread = recvfrom(ucast_recvfd, dwnld_chunk, SMALL_STR_LEN, 0, (__SA__*)&ucast_recv_addr, &len);
        if (nread < 0) {
            err_exit("recvfrom()");
            return -1;
        }
        if (nread < SMALL_STR_LEN) {
            // file download complete
            downloaded = 1;
        }
        if (fwrite(dwnld_chunk, sizeof(char), nread, fptr) < 0) {
            err_exit("fwrite()");
            return -1;
        }
    }
    fclose(fptr);

    strcpy(files_available[AVAILABLE_FILE_COUNT], filename);
    AVAILABLE_FILE_COUNT += 1;
    return 0;
}

/**
 * Send special file request message to all the groups that 
 * the peer has joined
 */
void send_file_request(char *filename) {
    peer_msg msg;
    strcpy(msg.msg_buff,filename);
    msg.msg_type = FILE_REQUEST;
    int len = __ADDR_LEN__;

    for (int i = 0; i < GRP_COUNT; i++) {
        if (grp_db[i].is_joined) {
            if (notify_grp(grp_db[i].grp_name, msg) == -1) {
                exit(EXIT_FAILURE);
            }
        }
    }

    if (fork() == 0) {
        peer_msg recvmsg;   
        alarm(60);

        int can_download = 0;
        while (to_poll) {
            int n = recvfrom(ucast_recvfd, &recvmsg, LARGE_STR_LEN, 0, (__SA__*)&ucast_recv_addr, &len);
            if (n < 0) {
                err_exit("recvfrom()");
                continue;
            }
            if (recvmsg.msg_type != FILE_REQUEST_RESPONSE) 
                continue;
            if (recvmsg.flag == 1) {
                can_download = 1;
                break;
            }
        }
        if (can_download) download_file(recvmsg, filename);
        else {
            printf("Either a timeout occurred or the file was not found with any of the peers\n\n");
        }
        to_poll = 1;
        exit(EXIT_SUCCESS);
    }
}

/**
 * Receive request for a file download and send the file in
 * chunks to the requesting peer
 */ 
void handle_file_request(peer_msg request_msg) {
    // request_msg contains the required filename in the buff field
    peer_msg response_msg;
    memset(&(response_msg.msg_buff), 0, LARGE_STR_LEN);
    strcpy(response_msg.sender_ip, unicast_ip);
    response_msg.msg_type = FILE_REQUEST_RESPONSE;
    int addr_len = __ADDR_LEN__;

    int response_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in response_addr;
    memset(&response_addr, 0, __ADDR_LEN__);

    response_addr.sin_family = AF_INET;
    response_addr.sin_port = htons(UCAST_PORT);
    response_addr.sin_addr.s_addr = inet_addr(request_msg.sender_ip);

    response_msg.flag = 0;
    for (int i = 0; i < AVAILABLE_FILE_COUNT; i++) {
        if (strcmp(request_msg.msg_buff, files_available[i]) == 0) {
            // found
            response_msg.flag = 1;
            break;
        }
    }

    int sent = sendto(response_fd, &response_msg, sizeof(peer_msg), 0, (__SA__*)&response_addr, __ADDR_LEN__);
    if (sent < 0) {
        err_exit("sendto()");
        exit(EXIT_FAILURE);
    }

    int rcvd = recvfrom(ucast_recvfd, &response_msg, sizeof(peer_msg), 0, (__SA__*)&ucast_recv_addr, &addr_len);
    if (response_msg.msg_type == DWNLD_FILE) {
        // start uploading the file to this peer
        FILE* fptr = fopen(request_msg.msg_buff, "rb");
        bool uploaded = false;
        while (!uploaded) {
            char upld_chunk[SMALL_STR_LEN];
            int upld_bytes = fread(upld_chunk, sizeof(char), SMALL_STR_LEN, fptr);
            if (upld_bytes < SMALL_STR_LEN) {
                uploaded = true;
            }
            int sent_bytes = sendto(response_fd, upld_chunk, upld_bytes, 0, (__SA__*)&response_addr, __ADDR_LEN__);
        }
        fclose(fptr);
    }
}


void print_command_list() {
    printf("List of avaiable commands:\n");
    printf("-> create-grp <grp_name> <mcast IP> <port>\n");
    printf("-> join <grp_name>\n");
    printf("-> leave <grp_name>\n");
    printf("-> search-grp <keyword>\n");
    printf("-> start-poll <grp_name>\n");
    printf("\n -- Type 'help' to refer to this list again -- \n\n");
}
/* ------------------------------------------------------------ */


int main(int argc, const char *argv[]) {

    signal(SIGALRM, poll_timeout_handler);

    bcastfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (bcastfd < 0) {
        err_exit("socket()");
        exit(EXIT_FAILURE);
    }
    memset(&bcast_addr, 0, __ADDR_LEN__);
    bcast_addr.sin_family = AF_INET;
    bcast_addr.sin_port = htons(BCAST_PORT);
    bcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    int bcastEnable = 1;
    if (setsockopt(bcastfd, SOL_SOCKET, SO_BROADCAST, &bcastEnable, sizeof(bcastEnable)) < 0) {
        err_exit("setsockopt()");
        exit(EXIT_FAILURE);
    }

    ucast_recvfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&ucast_recv_addr, 0, __ADDR_LEN__);
    ucast_recv_addr.sin_family = AF_INET;
    ucast_recv_addr.sin_port = htons(UCAST_PORT);
    ucast_recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(ucast_recvfd, (__SA__*)&ucast_recv_addr, __ADDR_LEN__) < 0) {
        err_exit("bind()");
        exit(EXIT_FAILURE);
    }

    FD_ZERO(&global_readset);
    FD_SET(0, &global_readset);

    printf("Enter your local IP, to be used for unicast communication: ");
    fgets(unicast_ip, IP_ADDR_LEN, stdin);
    unicast_ip[strlen(unicast_ip)-1] = 0;

    print_command_list();

    printf("\n");
    while (true) {
        printf("\n>> ");
        readset = global_readset;
        int num_ready_fd = select(maxfd+1, &readset, NULL, NULL, NULL);

        if (FD_ISSET(0, &readset)) {
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
                tokens[i++] = strdup(token);
                token = strtok(NULL, __SPACE__);
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
            else if (strcmp(tokens[0], "start-poll") == 0) {
                create_poll(tokens[1]);
            }
            else if (strcmp(tokens[0], "help") == 0) {
                print_command_list();
            }

            num_ready_fd -= 1;
            if (num_ready_fd == 0) continue;
            free(dup_cmd);
        }

        /* Send file list to all grp members every 1 minute */
        if (fork() == 0) {
            while (1) {
                if (advertise_file_list() == -1) {
                    exit(EXIT_FAILURE);
                }
                sleep(60);
            }
        }

        for (int i = 0; i < GRP_COUNT; i++) {
            if (grp_db[i].is_joined) {
                int sockfd = grp_db[i].recv_sockfd;
                struct sockaddr_in addr = grp_db[i].recv_addr;
                int len = __ADDR_LEN__;

                if (FD_ISSET(sockfd, &readset)) {
                    peer_msg msg;
                    int read = recvfrom(sockfd, &msg, LARGE_STR_LEN, 0, (__SA__*)&addr, &len);
                    // handle response for every message type
                    switch(msg.msg_type) {

                        case FILE_ADVERTISE:
                            {
                                char missing_files[LARGE_STR_LEN];
                                char *dup_list = strdup(msg.msg_buff);
                                char *token = strtok(dup_list, __SPACE__);

                                while (token) {
                                    // search for this token in the list of file available
                                    bool found = false;
                                    for (int i = 0; i < AVAILABLE_FILE_COUNT; i++) {
                                        if (strcmp(files_available[i], token) == 0) {
                                            found=true;
                                            break;
                                        }
                                    }
                                    if (!found) {
                                        // send request for all missing files 
                                        send_file_request(token);
                                    }
                                    token = strtok(NULL, __SPACE__);
                                }
                                free(dup_list);
                            }
                            break;

                        case FILE_REQUEST:
                            {
                                // respond with yes/no
                                handle_file_request(msg);
                                // forward request to other grps
                            }
                            break;
                        
                        case POLL:
                            {
                                // handle poll request
                                printf("A member initiated a new poll in the group <%s>\n", grp_db[i].grp_name);
                                printf("Poll Question:\n\t%s", msg.msg_buff);
                                char response[SMALL_STR_LEN];
                                printf("Enter response yes/no: ");
                                fgets(response, SMALL_STR_LEN, stdin);
                                response[strlen(response)-1] = 0;

                                // send response
                                peer_msg send_resp;
                                strcpy(send_resp.msg_buff, response);
                                strcpy(send_resp.sender_ip, unicast_ip);
                                send_resp.msg_type = POLL_RESPONSE;
                                int send_poll = sendto(sockfd, &send_resp, sizeof(peer_msg), 0, (__SA__*)&addr, len);

                                if (send_poll < 0) {
                                    err_exit("sendto()");
                                    exit(EXIT_FAILURE);
                                }
                            }
                            break;
                    }
                    // --------------------------------------
                    num_ready_fd -= 1;
                    if (num_ready_fd = 0) break;
                }
            }
        }
    }

    return 0;
}
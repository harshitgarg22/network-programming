#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define PORT 12345
#define MAX_BUF_LEN 256

char *itoa(int value, char *result, int base) {
    // check that the base if valid
    if (base < 2 || base > 36) {
        *result = '\0';
        return result;
    }

    char *ptr = result, *ptr1 = result, tmp_char;
    int tmp_value;

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + (tmp_value - value * base)];
    } while (value);

    // Apply negative sign
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    return result;
}

void die(char *s) {
    perror(s);
    exit(-1);
}

typedef struct childData {
    pid_t pid;
    struct childData *next;
} childData;

int numExist, numActive;                  // For parent proc
int numConnections, numConnectionsSoFar;  // For child proc

void hook_action_parent(char *actionBeingTaken, char *postAction) {
    printf("Number of children existing: %d\n", numExist);
    printf("Number of clients being handled: %d\n", numActive);
    printf("Action being taken: %s\n", actionBeingTaken);
    printf("Post action status: %s\n", postAction);
    printf("\n");
}

void handle_sigint_parent(int sig) {
    // Child will have a separate sigterm handler where it prints the number of connections handled.
    if (sig == SIGINT) {
        // Print number of children active
        printf("Currently active number of children: %d\n\n", numActive);
    } else {
        die("SIGINT not detected");
    }
}
void handle_sigchld(int sig) {
    if (sig == SIGCHLD) {
        wait(NULL);
        numExist--;
    } else {
        die("SIGCHLD not detected");
    }
}

void handle_sigint_child(int sig) {
    if (sig == SIGINT) {
        // Print number of client connections handled.
        printf("For child %d, Number of connections handled till now: %d\n", getpid(), numConnectionsSoFar);
    } else {
        die("SIGINT not detected");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        die("Incorrect arguments");
    }
    if (signal(SIGINT, handle_sigint_parent) == SIG_ERR) {
        die("Could not register SIGINT");
    }
    if (signal(SIGCHLD, handle_sigchld) == SIG_ERR) {
        die("Could not register SIGINT");
    }
    childData *myChildren = NULL;
    numActive = 0;
    numExist = 0;
    numConnections = 0;
    numConnectionsSoFar = 0;
    int MinSpareServers = atoi(argv[1]), MaxSpareServers = atoi(argv[2]), MaxRequestsPerChild = atoi(argv[3]);

    int sd;
    struct sockaddr_in name;
    sd = socket(AF_INET, SOCK_STREAM, 0);
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    name.sin_port = htons(PORT);

    if (bind(sd, (struct sockaddr *)&name, sizeof(name)) == -1) {
        die("Could not bind");
    }
    if (listen(sd, MaxRequestsPerChild)) {
        die("Could not listen");
    }
    printf("Listening on Port: %d...\n", PORT);

    pid_t mypid;

    int pcfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100;
    if (setsockopt(pcfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        die("Error");
    }
    struct sockaddr_un pcaddr;
    pcaddr.sun_family = AF_UNIX;
    strcpy(pcaddr.sun_path, "/tmp/mysock_p3_a2");
    unlink("/tmp/mysock_p3_a2");
    if (bind(pcfd, (struct sockaddr *)&pcaddr, sizeof(pcaddr)) == -1) {
        die("Unix socket couldn't bind in parent");
    }
    while (1) {
        int tempCount = 1;
        while ((numExist - numActive) <= MinSpareServers) {
            for (int i = 0; i < tempCount; ++i) {
                mypid = fork();
                if (mypid == 0) {
                    if (signal(SIGINT, handle_sigint_child) == SIG_ERR) {
                        die("Could not register SIGINT");
                    }
                    break;
                } else {
                    childData *newChild = (childData *)malloc(sizeof(childData));
                    newChild->next = myChildren;
                    newChild->pid = mypid;
                    myChildren = newChild;
                    char postAction[128];
                    strcpy(postAction, "Num of children increased by 1 to total of ");
                    char val[128];
                    numExist++;
                    strcat(postAction, itoa(numExist, val, 10));
                    hook_action_parent("Creating child", postAction);
                }
            }
            if (mypid == 0) break;
            if (tempCount < 32) {
                tempCount *= 2;
            }
            sleep(1);
        }
        if (mypid == 0) break;
        struct sockaddr_un client;
        unsigned int len;
        char data[1];
        if (recvfrom(pcfd, data, sizeof(data), 0, (struct sockaddr *)&client, &len) > 0) {
            if (data[0] == 'S') {
                ++numActive;
            } else {
                --numActive;
            }
        }
        while (numExist - numActive > MaxSpareServers) {
            pid_t kick = myChildren->pid;
            childData *delete = myChildren;
            myChildren = myChildren->next;
            free(delete);
            kill(kick, SIGKILL);
            char postAction[128];
            strcpy(postAction, "Num of children decreased by 1 to total of ");
            char val[128];
            numExist--;
            strcat(postAction, itoa(numExist, val, 10));
            hook_action_parent("Deleting child", postAction);
        }
    }
    if (mypid == 0) {
        struct sockaddr_in client;
        pcfd = socket(AF_UNIX, SOCK_DGRAM, 0);
        memset(&pcaddr, 0, sizeof(struct sockaddr_un));
        pcaddr.sun_family = AF_UNIX;
        // char addr_char[64];
        snprintf(pcaddr.sun_path, sizeof(pcaddr.sun_path), "/tmp/child_%d", getpid());
        // strncpy(&pcaddr.sun_path[1], "child", sizeof(pcaddr.sun_path) - 2);
        if (bind(pcfd, (struct sockaddr *)&pcaddr, sizeof(pcaddr)) == -1) {
            die("Unix socket couldn't bind in child");
        }
        for (;;) {
            int psd;
            unsigned int len;
            if ((psd = accept(sd, (struct sockaddr *)&client, &len)) == -1) {
                die("Could not accept connection");
            }
            char data[1];
            data[0] = 'S';
            struct sockaddr_un pcsendaddr;
            pcsendaddr.sun_family = AF_UNIX;
            strcpy(pcsendaddr.sun_path, "/tmp/mysock_p3_a2");
            sendto(pcfd, data, sizeof(data), 0, (struct sockaddr *)&pcsendaddr, sizeof(pcsendaddr));
            ++numConnections;
            ++numConnectionsSoFar;
            char buf[MAX_BUF_LEN];
            int cc;
            if ((cc = recv(psd, buf, sizeof(buf), 0)) == -1) {
                die("Could not receive messages");
            }
            char rcvaddr[32];
            inet_ntop(AF_INET, &(client.sin_addr), rcvaddr, sizeof(rcvaddr));
            printf("Child %d recevied data from %s:%d :\n %s", getpid(), rcvaddr, client.sin_port, buf);
            sleep(1);
            if (send(psd, "Reply", 6, 0) == -1) {
                die("Could not send");
            }
            close(psd);
            data[0] = 'F';
            sendto(pcfd, data, sizeof(data), 0, (struct sockaddr *)&pcsendaddr, sizeof(pcsendaddr));
            --numConnections;
            if (numConnectionsSoFar >= MaxRequestsPerChild) {
                printf("Flushing child %d because it exceeded maximum connection limit\n", getpid());
                kill(getppid(), SIGCHLD);
                break;
            }
        }
    }
    while (myChildren != NULL) {
        pid_t kick = myChildren->pid;
        childData *delete = myChildren;
        myChildren = myChildren->next;
        free(delete);
        kill(kick, SIGKILL);
    }
}
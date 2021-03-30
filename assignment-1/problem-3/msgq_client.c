#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "./msgq.h"

void createGroup(int msgqid, char *grpname, int msgqid_rcv) {
    msg_container crtgrp;
    pid_t mypid = getpid();
    crtgrp.mtype = 1;
    crtgrp.intent = CREATE_GROUP;
    strcpy(crtgrp.createGroup.groupName, grpname);
    crtgrp.src = mypid;

    if (msgsnd(msgqid, &crtgrp, sizeof(crtgrp) - sizeof(long), 0) == -1) {
        perror("createGroup msgsnd");
        return;
    }
    msg_container rcvmsg;
    printf("sizeof(crtgrp): %d, MSGQID %d\n", sizeof(crtgrp), msgqid_rcv);

    if (msgrcv(msgqid_rcv, &rcvmsg, sizeof(rcvmsg) - sizeof(long), 2, 0) == -1) {
        perror("Error while receiving message");
    }
    if (rcvmsg.createGroup.groupID == -1) {
        printf("Error while creating group: Group number limit reached\n");
    } else {
        printf("Successfully created group with group ID = %d\n", rcvmsg.createGroup.groupID);
    }
}

int requestGroups(int msgqid, char groupList[][MAX_GROUP_COUNT][MAX_GRP_NAME], int msgqid_rcv) {
    msg_container rqstGrps;
    pid_t mypid = getpid();
    rqstGrps.mtype = 1;
    rqstGrps.intent = LIST_GROUPS;
    rqstGrps.src = mypid;

    if (msgsnd(msgqid, &rqstGrps, sizeof(rqstGrps) - sizeof(long), 0) == -1) {
        perror("requestGroups msgsnd");
    }
    msg_container grpList;
    if (msgrcv(msgqid_rcv, &grpList, sizeof(grpList) - sizeof(long), 3, 0) == -1) {
        perror("requestGroups msgrcv");
    }
    int groupCount = grpList.requestGroup.groupCount;
    for (int i = 0; i < groupCount; ++i) {
        strcpy((*groupList)[i], grpList.requestGroup.groupList[i]);
    }
    return groupCount;
}

void joinGroup(int msgqid, int groupChoice, int msgqid_rcv) {
    msg_container joingrp;
    pid_t mypid = getpid();
    joingrp.joinGroup.groupID = groupChoice;
    joingrp.mtype = 1;
    joingrp.src = mypid;
    joingrp.intent = JOIN_GROUP;

    if (msgsnd(msgqid, &joingrp, sizeof(joingrp) - sizeof(long), 0) == -1) {
        perror("joinGroup msgsnd");
    }

    if (msgrcv(msgqid_rcv, &joingrp, sizeof(joingrp) - sizeof(long), 3, 0) == -1) {
        perror("joinGroup msgrcv");
    }

    if (joingrp.joinGroup.groupID == -1) {
        printf("Group does not exist\n");
    }
    if (joingrp.joinGroup.groupID == -2) {
        printf("No more space left in group\n");
    }
}

void sendPrivateMessage(int msgqid, UID senderID, char *msgText, int autoDel) {
    msg_container pvtmsg;
    pid_t mypid = getpid();
    pvtmsg.mtype = 1;
    pvtmsg.intent = SEND_PVT_MSG;
    pvtmsg.src = mypid;
    pvtmsg.sendMessage.dstn = senderID;
    strncpy(pvtmsg.sendMessage.msgText, msgText, MAX_MSG_SIZE);
    pvtmsg.sendMessage.msgTime = time(0);
    pvtmsg.sendMessage.autoDeleteTimeOut = autoDel;

    if (msgsnd(msgqid, &pvtmsg, sizeof(pvtmsg), 0) == -1) {
        perror("sendPrivateMessage msgsnd");
    }
}

void sendGroupMessage(int msgqid, int groupChoice, char *msgText, int autoDel) {
    msg_container grpmsg;
    pid_t mypid = getpid();
    grpmsg.mtype = 1;
    grpmsg.intent = SEND_GRP_MSG;
    grpmsg.src = mypid;
    grpmsg.groupMessage.dstn_gid = groupChoice;
    strncpy(grpmsg.groupMessage.msgText, msgText, MAX_MSG_SIZE);
    grpmsg.groupMessage.msgTime = time(0);
    grpmsg.groupMessage.autoDeleteTimeOut = autoDel;

    if (msgsnd(msgqid, &grpmsg, sizeof(msgqid), 0) == -1) {
        perror("sendGroupMessage msgsnd");
    }
}

int main(int argc, char *argv[]) {
    UID user_id;
    printf("Please login with your user id (0-99): ");
    scanf("%d", &user_id);

    int msgqid;
    key_t key;

    if ((key = ftok(MSGQ_PATH, 'C')) == -1) {
        perror("ftok()\n");
        exit(-1);
    }
    if ((msgqid = msgget(key, 0666 | IPC_CREAT)) == -1) {
        perror("msgget()\n");
        exit(-1);
    }
    printf("Message Queue ID: %d\n", msgqid);
    int choice = 0;
    UID myid = getpid();
    printf("Welcome, your id is %d.\n", myid);
    printf("Connecting to server...\n");

    pid_t child = fork();
    int msgqid_rcv;
    key_t key_rcv = myid;
    if ((msgqid_rcv = msgget(key_rcv, 0666 | IPC_CREAT)) == -1) {
        perror("msgget()\n");
        exit(-1);
    }

    if (child == 0) {
        msg_container rcvmsg;
        while (1) {
            if (msgrcv(msgqid_rcv, &rcvmsg, sizeof(rcvmsg) - sizeof(long), 2, 0) == -1) {
                perror("Error while receiving message");
            }
            switch (rcvmsg.intent) {
                case RCV_PVT_MSG: {
                    ;
                    printf("Message from %d: %s\n", rcvmsg.src, rcvmsg.rcvMessage.msgText);
                    break;
                }
                case RCV_GRP_MSG: {
                    ;
                    printf("From %d on %d: %s\n", rcvmsg.src, rcvmsg.rcvMessage.gid, rcvmsg.rcvMessage.msgText);
                }
                default:
                    break;
            }
            if (getppid() == 1) {
                exit(0);
            }
        }
    }

    do {
        printf(
            "What would you like to do?\n"
            "\t1. Create group\n"
            "\t2. List groups\n"
            "\t3. Join group\n"
            "\t4. Send private message\n"
            "\t5. Send group message\n"
            "\t0. Exit\n> ");
        scanf("%d", &choice);
        switch (choice) {
            case 1: {
                ;
                char grpName[MAX_GRP_NAME];
                printf("Please enter the name of your group (upto %d characters): ", MAX_GRP_NAME);
                scanf("%s", grpName);
                createGroup(msgqid, grpName, msgqid_rcv);
                break;
            }
            case 2: {
                ;
                char groupList[MAX_GROUP_COUNT][MAX_GRP_NAME];
                int groupCount = requestGroups(msgqid, &groupList, msgqid_rcv);
                for (int i = 0; i < groupCount; ++i) {
                    printf("\t%d. %s w/ \tid = %d\n", i + 1, (*groupList)[i], i);
                }
                break;
            }
            case 3: {
                ;
                int groupChoice;
                printf("So, which group do you want to join? (Enter groupID): ");
                scanf("%d", &groupChoice);
                joinGroup(msgqid, groupChoice, msgqid_rcv);
                break;
            }
            case 4: {
                ;
                UID senderID;
                printf("Enter the id of the user you want to send private message: ");
                scanf("%d", &senderID);
                printf("Enter message you wish to send: ");
                char msgText[MAX_MSG_SIZE];
                scanf("%s", msgText);
                printf("Set auto delete <T> option (0 for none): ");
                int autoDel;
                scanf("%d", &autoDel);

                sendPrivateMessage(msgqid, senderID, msgText, autoDel);

                break;
            }
            case 5: {
                ;
                int groupChoice;
                printf("So, which group do you want to send message to? (Enter groupID): ");
                scanf("%d", &groupChoice);

                printf("Enter message you wish to send: ");
                char msgText[MAX_MSG_SIZE];
                scanf("%s", msgText);

                printf("Set auto delete <T> option (0 for none)");
                int autoDel;
                scanf("%d", &autoDel);
                sendGroupMessage(msgqid, groupChoice, msgText, autoDel);

                break;
            }
            default:
                break;
        }
    } while (choice != 0);
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "./msgq.h"

void createGroup(int msgqid, char *grpname, int msgqid_rcv, UID user_id) {
    msg_container crtgrp;
    UID mypid = user_id;
    crtgrp.mtype = 1;
    crtgrp.intent = CREATE_GROUP;
    strcpy(crtgrp.createGroup.groupName, grpname);
    crtgrp.src = mypid;

    if (msgsnd(msgqid, &crtgrp, sizeof(crtgrp) - sizeof(long), 0) == -1) {
        perror("createGroup msgsnd");
        return;
    }
    msg_container rcvmsg;
    // printf("sizeof(crtgrp): %d, MSGQID %d\n", sizeof(crtgrp), msgqid_rcv);

    if (msgrcv(msgqid_rcv, &rcvmsg, sizeof(rcvmsg) - sizeof(long), 3, 0) == -1) {
        perror("Error while receiving message");
    }
    if (rcvmsg.createGroup.groupID == -1) {
        printf("Error while creating group: Group number limit reached\n");
    } else {
        printf("Successfully created group %s with group ID = %d\n", rcvmsg.createGroup.groupName, rcvmsg.createGroup.groupID);
    }
}

int requestGroups(int msgqid, char groupList[MAX_GROUP_COUNT][MAX_GRP_NAME], char joint[], int msgqid_rcv, UID user_id) {
    msg_container rqstGrps;
    UID mypid = user_id;
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
        strcpy((groupList)[i], grpList.requestGroup.groupList[i]);
        joint[i] = grpList.requestGroup.joint[i];
    }
    return groupCount;
}

void joinGroup(int msgqid, int groupChoice, int msgqid_rcv, UID user_id) {
    msg_container joingrp;
    UID mypid = user_id;
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

    if (joingrp.joinGroup.groupID == -3) {
        printf("Already a member of this group.\n");
    }
}

void sendPrivateMessage(int msgqid, UID senderID, char *msgText, int autoDel, UID user_id) {
    msg_container pvtmsg;
    UID mypid = user_id;
    pvtmsg.mtype = 1;
    pvtmsg.intent = SEND_PVT_MSG;
    pvtmsg.src = mypid;
    pvtmsg.sendMessage.dstn = senderID;
    strncpy(pvtmsg.sendMessage.msgText, msgText, MAX_MSG_SIZE);
    pvtmsg.sendMessage.msgTime = time(0);
    pvtmsg.sendMessage.autoDeleteTimeOut = autoDel;

    if (msgsnd(msgqid, &pvtmsg, sizeof(pvtmsg), MSG_NOERROR) == -1) {
        perror("sendPrivateMessage msgsnd");
    }
}

void sendGroupMessage(int msgqid, int groupChoice, char *msgText, int autoDel, UID user_id) {
    msg_container grpmsg;
    UID mypid = user_id;
    grpmsg.mtype = 1;
    grpmsg.intent = SEND_GRP_MSG;
    grpmsg.src = mypid;
    grpmsg.groupMessage.dstn_gid = groupChoice;
    strncpy(grpmsg.groupMessage.msgText, msgText, MAX_MSG_SIZE);
    grpmsg.groupMessage.msgTime = time(0);
    grpmsg.groupMessage.autoDeleteTimeOut = autoDel;

    if (msgsnd(msgqid, &grpmsg, sizeof(grpmsg), MSG_NOERROR) == -1) {
        perror("sendGroupMessage msgsnd");
    }
}

int main(int argc, char *argv[]) {
    UID user_id;
    do {
        printf("Please login with your user id (1000-1099): ");
        scanf("%d", &user_id);
    } while (user_id > 1099 || user_id < 1000);

    int msgqid;
    key_t key;

    if ((key = ftok(MSGQ_PATH, 'C')) == -1) {
        perror("ftok()\n");
        exit(-1);
    }
    if ((msgqid = msgget(key, 0666)) == -1) {
        printf("Could not connect to message queue. Check if server is running?\n");
        exit(-1);
    }
    msg_container joined;
    joined.mtype = 1;
    joined.intent = JOIN_SERVER;
    joined.src = user_id;
    time_t jointime = time(0);
    joined.joinTime.join_time = jointime;
    printf("Connecting to server...\n");
    if (msgsnd(msgqid, &joined, sizeof(joined) - sizeof(long), 0) == -1) {
        perror("msgsnd connecting to server error");
    }
    printf("Common Message Queue ID: %d\n", msgqid);
    int choice = 0;
    printf("Welcome, your UID is %d.\n", user_id);

    pid_t child = fork();
    int msgqid_rcv;
    key_t key_rcv = user_id;
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
                    if (jointime - rcvmsg.rcvMessage.msgTime <= rcvmsg.rcvMessage.autoDeleteTimeOut) {
                        printf("\nMessage from %d: %s\n", rcvmsg.src, rcvmsg.rcvMessage.msgText);
                    }
                    break;
                }
                case RCV_GRP_MSG: {
                    ;
                    if (jointime - rcvmsg.rcvMessage.msgTime <= rcvmsg.rcvMessage.autoDeleteTimeOut) {
                        printf("\nMessage from %d on gid %d: %s\n", rcvmsg.src, rcvmsg.rcvMessage.gid, rcvmsg.rcvMessage.msgText);
                    }
                }
                default:
                    break;
            }
            if (getppid() == 0) {
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
                getc(stdin);
                fgets(grpName, MAX_GRP_NAME, stdin);
                grpName[strlen(grpName) - 1] = '\0';
                // scanf("%[^\n]s", grpName);
                createGroup(msgqid, grpName, msgqid_rcv, user_id);
                break;
            }
            case 2: {
                ;
                char groupList[MAX_GROUP_COUNT][MAX_GRP_NAME];
                char joint[MAX_GROUP_COUNT];
                int groupCount = requestGroups(msgqid, groupList, joint, msgqid_rcv, user_id);
                printf("\tS.No.\tName\tGID\tJoined?\n");
                printf("\t------------------------------------\n");
                for (int i = 0; i < groupCount; ++i) {
                    printf("\t%d.\t%s\t%d\t%c\n", i + 1, groupList[i], i, joint[i]);
                }
                if (groupCount == 0) {
                    printf("<NIL>\n");
                }
                break;
            }
            case 3: {
                ;
                int groupChoice;
                printf("So, which group do you want to join? (Enter groupID): ");
                scanf("%d", &groupChoice);
                joinGroup(msgqid, groupChoice, msgqid_rcv, user_id);
                break;
            }
            case 4: {
                ;
                UID senderID;
                printf("Enter the id of the user you want to send private message: ");
                scanf("%d", &senderID);
                printf("Enter message you wish to send: ");
                char msgText[MAX_MSG_SIZE];
                getc(stdin);
                fgets(msgText, MAX_MSG_SIZE, stdin);
                // scanf("%[^\n]%*c", msgText);
                printf("Set auto delete <T> option (0 for none): ");
                int autoDel;
                scanf("%d", &autoDel);

                sendPrivateMessage(msgqid, senderID, msgText, autoDel, user_id);

                break;
            }
            case 5: {
                ;
                int groupChoice;
                printf("So, which group do you want to send message to? (Enter groupID): ");
                scanf("%d", &groupChoice);

                printf("Enter message you wish to send: ");
                char msgText[MAX_MSG_SIZE];
                getc(stdin);
                fgets(msgText, MAX_MSG_SIZE, stdin);
                // scanf("%[^\n]%*c", msgText);

                printf("Set auto delete <T> option (0 for none): ");
                int autoDel;
                scanf("%d", &autoDel);
                sendGroupMessage(msgqid, groupChoice, msgText, autoDel, user_id);

                break;
            }
            default:
                break;
        }
    } while (choice != 0);
}
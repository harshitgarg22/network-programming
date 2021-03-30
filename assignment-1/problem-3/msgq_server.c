#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

#include "./msgq.h"

int main(int argc, char *argv[]) {
    {
        char x;
        printf("Do you want to clear all existing message queues? (y/n) (Warning this will crash all active clients!) ");
        scanf("%c", &x);
        if (x == 'y') {
            if (!fork()) {
                execl("/usr/bin/ipcrm", "ipcrm", "--all=msg", NULL);
            }
            wait(NULL);
        }
    }
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
    printf("Message Queue ID: %d.\n", msgqid);
    msg_container rcvmsg;
    int size = sizeof(msg_container) - sizeof(rcvmsg.mtype);
    int groupCount = 0;
    char groupList[MAX_GROUP_COUNT][MAX_GRP_NAME];
    int groupMembers[MAX_GROUP_COUNT][MAX_GROUP_MEMBERS];
    for (int i = 0; i < MAX_GROUP_COUNT; ++i) {
        for (int j = 0; j < MAX_GROUP_MEMBERS; ++j) {
            groupMembers[i][j] = -1;
        }
    }
    int usrTime[MAX_USR];
    while (1) {
        if (msgrcv(msgqid, &rcvmsg, sizeof(rcvmsg) - sizeof(long), 1, MSG_NOERROR) == -1) {
            perror("ERROR while receiving message");
            return -1;
        }
        int msgqid_snd;
        key_t key_snd = rcvmsg.src;

        // printf("%d %d %s\n", rcvmsg.mtype, rcvmsg.intent, rcvmsg.createGroup.groupName);
        printf("Received message from UID %d with intent code %d.\n", rcvmsg.src, rcvmsg.intent);
        switch (rcvmsg.intent) {
            case JOIN_SERVER: {
                ;
                usrTime[rcvmsg.src - USR_OFFSET] = rcvmsg.joinTime.join_time;
                if ((msgqid_snd = msgget(key_snd, 0666 | IPC_CREAT)) == -1) {
                    perror("msgget()\n");
                    exit(-1);
                }
                break;
            }
            case CREATE_GROUP: {
                ;
                printf("Creating group named \"%s\" for %d\n", rcvmsg.createGroup.groupName, rcvmsg.src);
                msg_container sndmsg;
                sndmsg.mtype = 3;
                sndmsg.src = 1;
                if (groupCount >= MAX_GROUP_COUNT) {
                    sndmsg.createGroup.groupID = -1;
                } else {
                    strcpy(groupList[groupCount], rcvmsg.createGroup.groupName);
                    groupMembers[groupCount][0] = rcvmsg.src;
                    strcpy(sndmsg.createGroup.groupName, groupList[groupCount]);
                    sndmsg.createGroup.groupID = groupCount;
                    ++groupCount;
                }
                if ((msgqid_snd = msgget(key_snd, 0666)) == -1) {
                    perror("msgget()\n");
                    exit(-1);
                }
                if (msgsnd(msgqid_snd, &sndmsg, sizeof(sndmsg) - sizeof(long), 0) == -1) {
                    perror("CREATE_GROUP msgsnd");
                }
                printf("Sent on MID %d\n", msgqid_snd);
                break;
            }
            case LIST_GROUPS: {
                msg_container sndmsg;
                sndmsg.mtype = 3;
                sndmsg.src = 0;
                sndmsg.requestGroup.groupCount = groupCount;
                for (int i = 0; i < groupCount; ++i) {
                    strcpy(sndmsg.requestGroup.groupList[i], groupList[i]);
                }

                for (int i = 0; i < groupCount; ++i) {
                    sndmsg.requestGroup.joint[i] = 'N';
                    for (int j = 0; j < MAX_GROUP_MEMBERS; ++j) {
                        if (groupMembers[i][j] == rcvmsg.src) {
                            sndmsg.requestGroup.joint[i] = 'Y';
                            break;
                        }
                        if (groupMembers[i][j] == -1) {
                            break;
                        }
                    }
                }
                if ((msgqid_snd = msgget(key_snd, 0666)) == -1) {
                    perror("msgget()\n");
                    exit(-1);
                }
                if (msgsnd(msgqid_snd, &sndmsg, sizeof(sndmsg) - sizeof(long), 0) == -1) {
                    perror("LIST_GROUPS msgsnd");
                }
                printf("Sent on MID %d\n", msgqid_snd);
                break;
            }
            case JOIN_GROUP: {
                msg_container sndmsg;
                sndmsg.mtype = 3;
                sndmsg.src = 1;
                if (rcvmsg.joinGroup.groupID >= groupCount) {
                    sndmsg.joinGroup.groupID = -1;
                } else {
                    int flag = 0;
                    for (int i = 0; i < MAX_GROUP_MEMBERS; ++i) {
                        if (groupMembers[rcvmsg.joinGroup.groupID][i] == rcvmsg.src) {
                            flag = 2;
                            break;
                        }
                        if (groupMembers[rcvmsg.joinGroup.groupID][i] == -1) {
                            groupMembers[rcvmsg.joinGroup.groupID][i] = rcvmsg.src;
                            flag = 1;
                            break;
                        }
                    }
                    if (flag == 1) {
                        sndmsg.joinGroup.groupID = rcvmsg.joinGroup.groupID;
                    } else if (flag == 0) {
                        sndmsg.joinGroup.groupID = -2;
                    } else {
                        sndmsg.joinGroup.groupID = -3;
                    }
                }
                if ((msgqid_snd = msgget(key_snd, 0666)) == -1) {
                    perror("msgget()\n");
                    exit(-1);
                }
                if (msgsnd(msgqid_snd, &sndmsg, sizeof(sndmsg) - sizeof(long), 0) == -1) {
                    perror("JOIN_GROUP msgsnd");
                }
                printf("Sent on MID %d\n", msgqid_snd);
                break;
            }
            case SEND_PVT_MSG: {
                msg_container sndmsg;
                sndmsg.intent = RCV_PVT_MSG;
                sndmsg.mtype = 2;
                sndmsg.src = rcvmsg.src;
                sndmsg.rcvMessage.gid = -1;
                sndmsg.rcvMessage.autoDeleteTimeOut = rcvmsg.sendMessage.autoDeleteTimeOut;
                sndmsg.rcvMessage.msgTime = rcvmsg.sendMessage.msgTime;
                strcpy(sndmsg.rcvMessage.msgText, rcvmsg.sendMessage.msgText);
                int msgqid_rcv;
                key_t key_rcv = rcvmsg.sendMessage.dstn;
                printf("Sending to UID %d\n", rcvmsg.sendMessage.dstn);
                if ((msgqid_rcv = msgget(key_rcv, 0666 | IPC_CREAT)) == -1) {
                    perror("msgget()\n");
                    exit(-1);
                }
                pid_t send_pvt_msg = fork();
                if (send_pvt_msg == 0) {
                    if (rcvmsg.sendMessage.autoDeleteTimeOut != 0) {
                        if (usrTime[rcvmsg.src] - rcvmsg.sendMessage.msgTime <= rcvmsg.sendMessage.autoDeleteTimeOut) {
                            msgsnd(msgqid_rcv, &sndmsg, sizeof(sndmsg) - sizeof(long), 0);
                            printf("Sent on MID %d\n", msgqid_snd);
                        }
                    } else {
                        msgsnd(msgqid_rcv, &sndmsg, sizeof(sndmsg) - sizeof(long), 0);
                        printf("Sent on MID %d\n", msgqid_snd);
                    }
                    exit(0);
                }
                printf("Sent on MID %d\n", msgqid_snd);
                break;
            }
            case SEND_GRP_MSG: {
                ;
                msg_container sndmsg;
                sndmsg.mtype = 2;
                sndmsg.src = rcvmsg.src;
                sndmsg.intent = RCV_GRP_MSG;
                strcpy(sndmsg.rcvMessage.msgText, rcvmsg.groupMessage.msgText);
                sndmsg.rcvMessage.gid = rcvmsg.groupMessage.dstn_gid;
                sndmsg.rcvMessage.msgTime = rcvmsg.sendMessage.msgTime;
                sndmsg.rcvMessage.autoDeleteTimeOut = rcvmsg.sendMessage.autoDeleteTimeOut;
                int msgqid_rcv;
                key_t key_rcv;

                int memCount = 0;
                for (int i = 0; i < MAX_GROUP_MEMBERS; ++i) {
                    if (groupMembers[rcvmsg.groupMessage.dstn_gid][i] != -1) {
                        ++memCount;
                    } else {
                        break;
                    }
                }
                int flag = 0;
                for (int i = 0; i < memCount; ++i) {
                    if (groupMembers[rcvmsg.groupMessage.dstn_gid][i] == rcvmsg.src) {
                        flag = 1;
                        break;
                    }
                }
                if (flag == 0) {
                    printf("UID %d does not belong to the gid %d. Silently discarding.\n", rcvmsg.src, rcvmsg.groupMessage.dstn_gid);
                    break;
                }
                for (int i = 0; i < memCount; ++i) {
                    key_rcv = groupMembers[rcvmsg.groupMessage.dstn_gid][i];
                    if ((msgqid_rcv = msgget(key_rcv, 0666)) == -1) {
                        perror("msgget()\n");
                        exit(-1);
                    }
                    pid_t send_pvt_msg = fork();
                    if (send_pvt_msg == 0) {
                        if (rcvmsg.sendMessage.autoDeleteTimeOut != 0) {
                            if (usrTime[rcvmsg.src] - rcvmsg.sendMessage.msgTime <= rcvmsg.sendMessage.autoDeleteTimeOut) {
                                msgsnd(msgqid_rcv, &sndmsg, sizeof(sndmsg) - sizeof(long), 0);
                                printf("Sent on MID %d\n", msgqid_snd);
                            }
                        } else {
                            msgsnd(msgqid_rcv, &sndmsg, sizeof(sndmsg) - sizeof(long), 0);
                            printf("Sent on MID %d\n", msgqid_snd);
                        }
                        exit(0);
                    }
                }
                break;
            }

            default:
                break;
        }
    }
}
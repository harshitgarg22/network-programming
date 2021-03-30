#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "./msgq.h"

int main(int argc, char *argv[]) {
    int msgqid;
    key_t key;

    if ((key = ftok(MSGQ_PATH, 'C')) == -1) {
        perror("ftok()\n");
        exit(-1);
    }
    if ((msgqid = msgget(key, 0666)) == -1) {
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
    while (1) {
        if (msgrcv(msgqid, &rcvmsg, size, 1, 0) == -1) {
            perror("ERROR while receiving message");
            return -1;
        }
        int msgqid_snd;
        key_t key_snd = rcvmsg.src;

        if ((msgqid_snd = msgget(key_snd, 0666 | IPC_CREAT)) == -1) {
            perror("msgget()\n");
            exit(-1);
        }

        printf("%d %d %s\n", rcvmsg.mtype, rcvmsg.intent, rcvmsg.createGroup.groupName);
        printf("Received message.\n");
        switch (rcvmsg.intent) {
            case CREATE_GROUP: {
                printf("Create group message received from %d\n", rcvmsg.src);
                msg_container sndmsg;
                sndmsg.mtype = 3;
                sndmsg.src = 1;
                if (groupCount >= MAX_GROUP_COUNT) {
                    sndmsg.createGroup.groupID = -1;
                } else {
                    strcpy(groupList[groupCount], rcvmsg.createGroup.groupName);
                    for (int i = 0; i < MAX_GROUP_MEMBERS; ++i) {
                        if (groupMembers[groupCount][i] == -1) {
                            groupMembers[groupCount][i] = rcvmsg.src;
                        }
                    }
                    sndmsg.createGroup.groupID = groupCount;
                    ++groupCount;
                }
                if (msgsnd(msgqid_snd, &sndmsg, sizeof(sndmsg) - sizeof(long), 0) == -1) {
                    perror("CREATE_GROUP msgsnd");
                }
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
                if (msgsnd(msgqid_snd, &sndmsg, sizeof(sndmsg) - sizeof(long), 0) == -1) {
                    perror("LIST_GROUPS msgsnd");
                }
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
                        if (groupMembers[rcvmsg.joinGroup.groupID][i] == -1) {
                            groupMembers[rcvmsg.joinGroup.groupID][i] = rcvmsg.src;
                            flag = 1;
                        }
                    }
                    if (flag == 1) {
                        sndmsg.joinGroup.groupID = rcvmsg.joinGroup.groupID;
                    } else {
                        sndmsg.joinGroup.groupID = -2;
                    }
                }
                if (msgsnd(msgqid_snd, &sndmsg, sizeof(sndmsg) - sizeof(long), 0) == -1) {
                    perror("JOIN_GROUP msgsnd");
                }
                break;
            }
            case SEND_PVT_MSG: {
                msg_container sndmsg;
                sndmsg.intent = RCV_PVT_MSG;
                sndmsg.mtype = 2;
                sndmsg.src = rcvmsg.src;
                sndmsg.rcvMessage.gid = -1;
                strcpy(sndmsg.rcvMessage.msgText, rcvmsg.sendMessage.msgText);
                int msgqid_rcv;
                key_t key_rcv = rcvmsg.sendMessage.dstn;

                if ((msgqid_rcv = msgget(key_rcv, 0666 | IPC_CREAT)) == -1) {
                    perror("msgget()\n");
                    exit(-1);
                }

                pid_t send_pvt_msg = fork();
                if (send_pvt_msg == 0) {
                    if (rcvmsg.sendMessage.autoDeleteTimeOut != 0) {
                        while (kill(rcvmsg.sendMessage.dstn, 0) == ESRCH && rcvmsg.sendMessage.autoDeleteTimeOut != 0) {
                            sleep(1);
                            rcvmsg.sendMessage.autoDeleteTimeOut--;
                        }
                        if (rcvmsg.sendMessage.autoDeleteTimeOut != 0) {
                            msgsnd(msgqid_rcv, &sndmsg, sizeof(sndmsg) - sizeof(long), 0);
                        }
                    } else {
                        msgsnd(msgqid_rcv, &sndmsg, sizeof(sndmsg) - sizeof(long), 0);
                    }
                    exit(0);
                }
                break;
            }
            case SEND_GRP_MSG: {
                msg_container sndmsg;
                sndmsg.mtype = 2;
                sndmsg.src = rcvmsg.src;
                sndmsg.intent = RCV_GRP_MSG;
                strcpy(sndmsg.rcvMessage.msgText, rcvmsg.groupMessage.msgText);
                sndmsg.rcvMessage.gid = rcvmsg.groupMessage.dstn_gid;

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

                for (int i = 0; i < memCount; ++i) {
                    key_rcv = groupMembers[rcvmsg.groupMessage.dstn_gid][i];
                    if ((msgqid_rcv = msgget(key_rcv, 0666 | IPC_CREAT)) == -1) {
                        perror("msgget()\n");
                        exit(-1);
                    }
                    pid_t send_pvt_msg = fork();
                    if (send_pvt_msg == 0) {
                        if (rcvmsg.sendMessage.autoDeleteTimeOut != 0) {
                            while (kill(rcvmsg.sendMessage.dstn, 0) == ESRCH && rcvmsg.sendMessage.autoDeleteTimeOut != 0) {
                                sleep(1);
                                rcvmsg.sendMessage.autoDeleteTimeOut--;
                            }
                            if (rcvmsg.sendMessage.autoDeleteTimeOut != 0) {
                                msgsnd(msgqid_rcv, &sndmsg, sizeof(sndmsg) - sizeof(long), 0);
                            }
                        } else {
                            msgsnd(msgqid_rcv, &sndmsg, sizeof(sndmsg) - sizeof(long), 0);
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
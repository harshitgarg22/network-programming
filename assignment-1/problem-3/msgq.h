#ifndef MSGQ_H_INLCUDED
#define MSGQ_H_INCLUDED
#include <sys/types.h>
#include <unistd.h>

#define MSGQ_PATH "./msgq_server.c"
#define MSG_MAX_SIZE 2048
#define MAX_GROUP_COUNT 12
#define MAX_MSG_SIZE 256
#define MAX_GRP_NAME 32
#define MAX_MSG_HOLD 16
#define MAX_GROUP_MEMBERS 16

typedef int UID;

typedef enum msg_intent {
    CREATE_GROUP,
    LIST_GROUPS,
    JOIN_GROUP,
    SEND_PVT_MSG,
    SEND_GRP_MSG,
    RCV_PVT_MSG,
    RCV_GRP_MSG
} msg_intent;

typedef struct create_group {
    char groupName[MAX_GRP_NAME];
    int groupID;
} create_group;

typedef struct request_group {
    char groupList[MAX_GROUP_COUNT][MAX_GRP_NAME];
    int groupCount;
} request_group;

typedef struct join_group {
    int groupID;
} join_group;

typedef struct send_message {
    UID dstn;
    char msgText[MAX_MSG_SIZE];
    time_t msgTime;
    time_t autoDeleteTimeOut;
} send_message;

typedef struct group_message {
    int dstn_gid;
    char msgText[MAX_MSG_SIZE];
    time_t msgTime;
    time_t autoDeleteTimeOut;
} group_message;

typedef struct rcv_message {
    char msgText[MAX_MSG_SIZE];
    int gid;
} rcv_message;

typedef struct msg_container {
    long int mtype;
    UID src;
    msg_intent intent;
    union {
        create_group createGroup;
        request_group requestGroup;
        join_group joinGroup;
        send_message sendMessage;
        group_message groupMessage;
        rcv_message rcvMessage;
    };
} msg_container;

#endif
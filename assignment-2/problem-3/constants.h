#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <setjmp.h>

#define BCAST_PORT              8080        /* Fixed port used for broadcast communication */
#define UCAST_PORT              8081        /* Fixed port used for unicast communication */
#define IP_ADDR_LEN             16          /* Length of an IPv4 address */
#define MAX_GRPS_ALLOWED        10          /* Maximum number of groups that a peer can join */
#define MAX_CMD_LEN             32          /* Maximum length of command that user can enter on prompt */
#define MAX_FILES               64          /* Maximum number of files that a peer can keep downloaded locally */
#define SMALL_STR_LEN           32          
#define LARGE_STR_LEN           256

#define __SPACE__               " "
#define __ADDR_LEN__            sizeof(struct sockaddr_in)
#define __SA__                  struct sockaddr
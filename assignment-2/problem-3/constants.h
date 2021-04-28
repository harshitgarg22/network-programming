#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <setjmp.h>

#define BCAST_PORT              8080
#define UCAST_PORT              8081
#define MAX_ADDR_LEN            64
#define MAX_GRPS_ALLOWED        10
#define MAX_CMD_LEN             32          /* Maximum length of command that user can enter on prompt */
#define SMALL_STR_LEN           32          /* Maximum length of any general string taken as input */
#define LARGE_STR_LEN           512
#define MAX_FILES               64          /* Maximum number of files that a peer can keep downloaded locally */

#define __SPACE__       " "
#define __ADDR_LEN__    sizeof(struct sockaddr_in)
#define __SA__          struct sockaddr
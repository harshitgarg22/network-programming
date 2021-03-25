/*
Clustershell server protocol:
1. Server is run on 1 pre selected IP
2. Server keeps waiting for connection from clients, and maintains a list of connected clients
3. Upon recieving a command from any client, it coordinates the connected machines to execute it
4. Returns final output to the client
*/

////////////////////////////////////////////
// Included libraries
////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

////////////////////////////////////////////
// Constants
////////////////////////////////////////////

// maximum number of clients that will probably connect at a time
#define MAX_NUM_OF_CONNECTED_CLIENTS 10

////////////////////////////////////////////
// Data Structures
////////////////////////////////////////////

// Stores information about a connected client
typedef struct connected_client_node{
    char* name;
    
}CONNECTED_CLIENT_NODE;

// Stores all the connected clients in a list
typedef struct connected_clients{
    int num; // number of connected clients
    CONNECTED_CLIENT_NODE list[MAX_NUM_OF_CONNECTED_CLIENTS];
}CONNECTED_CLIENT;

////////////////////////////////////////////
// Functions
////////////////////////////////////////////

int main(){



    return 0;
}
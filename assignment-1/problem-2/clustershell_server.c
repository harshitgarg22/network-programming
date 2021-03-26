/*
Clustershell server protocol:
1. Server is run on 1 pre selected IP
2. Server keeps waiting for connection from clients, and maintains a list of connected clients
3. Upon recieving a command from any client, it coordinates the connected machines to execute it
4. Returns final output to the client

Assumptions:
1. All clients listed in the config file connect in the beginning itself and none of them leave before all commands are over
2. There are no commands that require manual user input from the shell (stdin)
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
// server port
#define SERV_PORT 12038
// this goes in the listen() call
#define MAX_CONNECTION_REQUESTS_IN_QUEUE 10

////////////////////////////////////////////
// Data Structures
////////////////////////////////////////////

// Stores information about a connected client
typedef struct connected_client_node{
    char* name;
    struct sockaddr_in client_addr;
    int clientfd;
}CONNECTED_CLIENT_NODE;

// Stores all the connected clients in a list
typedef struct connected_clients{
    int num; // number of connected clients
    CONNECTED_CLIENT_NODE list[MAX_NUM_OF_CONNECTED_CLIENTS];
}CONNECTED_CLIENTS;

////////////////////////////////////////////
// Functions
////////////////////////////////////////////


/*
    Accepts new clients, parses new commands, deploys commands to the respective machines
    then returns output to the requesting node.
*/
int main(int argc, char* argv[]){
    
    // create the main listening socket for the server
    int serv_socket = socket(PF_INET, SOCK_STREAM, 0);
    
    // make address structure to bind the main listening server to the server port
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, size(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = SERV_PORT;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind the main listerning socket to the server port
    bind(serv_socket, (struct sockaddr*)&serv_addr, size(serv_addr));

    // start listening at the port
    listen(serv_socket, MAX_CONNECTION_REQUESTS_IN_QUEUE);

    // server setup
    CONNECTED_CLIENTS clients;
    clients.num = 0;

    while (true) {
        // accept a new client and fill details in the client list structure
        if (clients.list[clients.num].clientfd = accept(serv_socket, (struct sockaddr*)&clients.list[clients.num].client_addr, size(clients.list[clients.num].client_addr)) < 0) {
            perror ("accept");
            return -1;
        }        

        clients.num++;
    }

    return 0;
}
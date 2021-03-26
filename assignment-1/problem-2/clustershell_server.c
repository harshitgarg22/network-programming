/*
Clustershell server protocol:
1. Server is run on 1 pre selected IP
2. Server keeps waiting for connection from clients, and maintains a list of connected clients
3. Upon recieving a command from any client, it coordinates the connected machines to execute it
4. Returns final output to the client

Message Design:
6 digit header + message, as follows:
To server: [c/o][mlength][command/output string]
To client: [c/o][mlength][(if c) i][(if c)ilength][input string][command/output string]
c - command
o - output
mlength - 5 digit number signifying length of rest of message
(only if command to client) i - input 
(only if command to client) ilength - 3 digit number, length of input string

Assumptions:
1. All clients listed in the config file connect in the beginning itself and none of them leave before all commands are over
2. There are no commands that require manual user input from the shell (stdin)
3. Commands and outputs are of maximum string length 99999 include all ending characters and newlines. 
(If output is of greater length, then it will be cutoff at that point.)
4. Nodes are named as n1, n2, n3, ... nN.

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
#include <string.h>

////////////////////////////////////////////
// Constants
////////////////////////////////////////////

// maximum number of clients that will probably connect at a time
#define MAX_NUM_OF_CONNECTED_CLIENTS 10
// server port
#define SERV_PORT 12038
// this goes in the listen() call
#define MAX_CONNECTION_REQUESTS_IN_QUEUE 10
// size of header of messages (according to message format)
#define HEADER_SIZE 6

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
Returns the number of lines in the config file
*/
int num_lines_in_config(){
    printf ("\n\nfinish num_lines_in_config\n\n");

    return 0;
}


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

    // find total number of clients
    int num_of_clients = num_lines_in_config();

    // accept connections from all the clients and fill details in the client list structure
    for (int i = 0; i < num_of_clients; i++) {
        if (clients.list[clients.num].clientfd = accept(serv_socket, (struct sockaddr*)&clients.list[clients.num].client_addr, size(clients.list[clients.num].client_addr)) < 0) {
            perror("accept");
            return -1;
        }
        // TODO: fill in the name
        clients.num++;
    }

    // listen for messages on each of the sockets and handle the commands received
    while (true){
        // step 1: iterate over each of the sockets to check if we have any command
        char header_buf[HEADER_SIZE + 1];
        int commander_idx;
        for (int commander_idx = 0; commander_idx < clients.num; commander_idx++){
            int bytes_read = read (clients.list[commander_idx].clientfd, &header_buf, HEADER_SIZE);
            if (bytes_read == 0) // client i hasn't sent anything
                continue;
            else if (bytes_read == HEADER_SIZE) // client i has sent a command
                break;
            else { // bytes read is less than the minimum for any message (bytes in the header), probable cause network is network error
                printf ("\nPossible network error encountered. Exiting application.\n");
                return 0;
            }
        }
        // step 2: handle the command
        if (header_buf[0] == 'c'){ // command received, according to message format
            header_buf[HEADER_SIZE] = '\0'; // convert header to string
            int command_size = atoi(header_buf+1); //get the size of the message (command) as int

            // read the command as string into command_buffer from the commander node socket
            char command_buffer[command_size+1]; 
            int bytes_read = read (clients.list[commander_idx].clientfd, &header_buf, command_size);
            command_buffer[command_size] = '\0';

            // parse the command

            
            // execute the command on various nodes


            // return output to the commander node socket

        }
        else { // message format not followed, 'c' not found as first letter
            printf ("\nPossible application or network error detected. Exiting application.\n");
        }

    }
    return 0;
}
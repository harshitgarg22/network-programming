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
mlength - 5 digit number signifying length of command/output string
(only if command to client) i - input 
(only if command to client) ilength - 5 digit number, length of input string

Assumptions:
1. All clients listed in the config file connect in the beginning itself and none of them leave before all commands are over
2. There are no commands that require manual user input from the shell (stdin)
3. Commands and outputs are of maximum string length 99999 include all ending characters and newlines. 
(If output is of greater length, then it will be cutoff at that point.)
4. Nodes are named as n1, n2, n3, ... nN.
5. Nodes are listed in order in config file and no number is missing

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
#include <ctype.h>

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
// maximum number of piped commands in a big main command sent by a node for distributed execution
#define MAX_NUMBER_OF_PIPED_COMMANDS 10
// path to config file
#define CONFIG_PATH "config"
// max size of line in config file
#define MAX_SIZE_OF_LINE_IN_CONFIG 25
// the port on which client runs its executioner process
#define CLIEX_PORT 12345


////////////////////////////////////////////
// Data Structures
////////////////////////////////////////////

// Stores information about a connected client
typedef struct connected_client_node{
    int nodenum;
    struct sockaddr_in client_addr;
    int clientfd;
}CONNECTED_CLIENT_NODE;

// Stores all the connected clients in a list
typedef struct connected_clients{
    int num; // number of connected clients
    CONNECTED_CLIENT_NODE list[MAX_NUM_OF_CONNECTED_CLIENTS];
}CONNECTED_CLIENTS;

// stores information about a command
typedef struct command{
    int nodenum;
    char* command;
    char* input;
}COMMAND;

// stores the parsed form of commands from a remote node
typedef struct parsed_commands{
    int num;
    COMMAND list[MAX_NUMBER_OF_PIPED_COMMANDS];
}PARSED_COMMANDS;

// stores the parsed form of the config file
typedef struct node_list{
    int num;
    char* ip[MAX_NUM_OF_CONNECTED_CLIENTS];
}NODE_LIST;

////////////////////////////////////////////
// Functions
////////////////////////////////////////////

/* 
    reads config file into the node_list data structure
*/
NODE_LIST get_nodelist_from_config(char* path){
    FILE* config_fd = fopen (path,"r");
    if (config_fd == NULL){
        printf ("Error opening config file \n");
        exit (1);
    }

    NODE_LIST nodelist;
    nodelist.num = 0;
    char* line = NULL;
    size_t bytes_read;
    ssize_t n = 0;
    while ((bytes_read = getline(&line, &n, config_fd)) != -1) {
        if (nodelist.num == MAX_NUM_OF_CONNECTED_CLIENTS){
            printf("config file has too many listings, please increase MAX_NUM_OF_CONNECTED_CLIENTS in clustershell_server.c. Exiting.\n");
            exit(1);
        }
        if (line[bytes_read-1] == '\n')
            line[bytes_read-1] = '\0';
        int idx = atoi(strtok(line, " ") + 1) - 1;
        nodelist.ip[idx] = strdup(strtok(NULL, " "));
        nodelist.num++;
        free(line);
    }
    return nodelist;
}

/*
    Parses the command taken as string from the socket and converts it into the PARSED_COMMANDS structure
*/
PARSED_COMMANDS parse_command (char* command, int commander_nodenum){
    // move pointers and \0 around to trim trailing and leading white spaces
    char* command_s;
    char* command_str = command_s = strdup(command);
    while (*command_str == ' ')
        command_str++;
    int command_length = strlen (command_str);
    while (command_str[command_length-1] == ' ')
        command_length--;
    command_str[command_length] = '\0';

    // initiate the cmds structure
    PARSED_COMMANDS cmds;
    cmds.num = 0;
    cmds.list[0].input = NULL;

    // process the command one by one after tokenising it on '|'
    int i = 0;
    char* token = strtok (command_str, "|");
    while (token != NULL){
        // trim leading white space
        while (*token == ' ' || *token == '\t' || *token == '\n')
            token++;
        
        if (token[0] == 'n'){ // "n[something]" case (node is mentioned)
            if (token [1] == '*' && token [2] == '.'){ // "n*." case
                cmds.list[i].nodenum = 0;
                cmds.list[i].command = strdup(token+3);
                cmds.list[i].input = NULL;
                
                cmds.num++;
                i++;
                token = strtok (NULL, "|");
                continue;
            }
            else if (isdigit(token[1])){ // "n[number]." case
                int j = 2;
                while (isdigit(token[j]))
                    j++;
                if (token[j] == '.'){
                    token[j] = '\0';
                    cmds.list[i].nodenum = atoi(token + 1);
                    cmds.list[i].command = strdup(token + j + 1);
                    cmds.list[i].input = NULL;
                    token[j] = '.';
                    
                    cmds.num++;
                    i++;
                    token = strtok (NULL, "|");
                    continue;
                }
            }
        }
        // no node mentioned, so codecum should be the commanding node itself
        cmds.list[i].nodenum = commander_nodenum;
        cmds.list[i].command = strdup(token);
        cmds.list[i].input = NULL;
    
        cmds.num++;
        i++;
        token = strtok (NULL, "|");
    }

    // we created duplicate string, so free the memory
    free(command_s);
    return cmds;
}

/*
    returns the header string according to message format, given "c" or "o" and the size of the rest of message
*/
char* get_header_str(char* co, int size){
    int num_length = 0;
    int s = size;
    while (s != 0){
        s = s/10;
        num_length++;
    }
    if (size == 0)
        num_length = 1;
    if (num_length > HEADER_SIZE - 1){
        printf ("output is too long, exiting.\n");
        exit (1);
    }
    char* zeroes = malloc((HEADER_SIZE - 1 - num_length + 1)* sizeof (char));
    for (int i = 0; i < HEADER_SIZE - num_length - 1; i++)
        zeroes[i] = '0';
    zeroes[HEADER_SIZE-num_length - 1] = '\0';
    char* header_str = malloc ((num_length + 1 + 1) * sizeof (char));
    sprintf(header_str, "%s%s%d", co, zeroes, size);
    return header_str;
}

/*
    returns the node num of the given address from the nodelist parsed from config file
*/
int get_node_num(struct in_addr node_addr, NODE_LIST nodelist){
    struct in_addr cmp_addr;
    for (int i = 0; i < nodelist.num; i++){
        if (inet_aton(nodelist.ip[i], &cmp_addr) == 0){
            printf ("Invalid address from node, error. Exiting.\n");
            exit(1);
        }
        if (cmp_addr.s_addr == node_addr.s_addr){
            return (i+1);
        }
    }
    return -1;
}

/*
    Free all the strings pointed to internally by parsed commands structure
*/
void free_parsed_commands(PARSED_COMMANDS cmds){
    for (int i = 0; i < cmds.num; i++){
        free(cmds.list[i].command);
        free(cmds.list[i].input);
    }
    return;
}

/*
    execute the given command on its node and return the output
    TODO: Add support for n*
*/
char* execute_on_remote_node(COMMAND cmd, CONNECTED_CLIENTS clients) {
    // find the client address for the command node, change the port, and make a connection to the client executioner
    int i;
    for (i = 0; i < clients.num; i++){
        if (clients.list[i].nodenum == cmd.nodenum)
            break;
    }
    struct sockaddr_in cliex_addr = clients.list[i].client_addr;
    cliex_addr.sin_port = CLIEX_PORT;
    int cliex_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connect(cliex_fd, (struct sockaddr*)&cliex_addr, sizeof(cliex_addr)) == -1){
        printf("Couldn't connect to client executioner. Exiting application.\n");
        perror("connect");
        exit(1);
    }
    printf ("here3\n");
    // compose the command message
    char* header_str = get_header_str("c", strlen(cmd.command));
    char* inphdr_str = NULL;
    int input_length = 0;
    if (cmd.input != NULL)
        input_length = strlen(cmd.input);
    inphdr_str = get_header_str("i", input_length);
    printf("input header: %s\n", inphdr_str);
    printf("cmd header: %s\n", header_str);
    printf("cmd: %s\n", cmd.command);
    printf("inp: %s\n", cmd.input);
    char* msg = malloc((strlen(inphdr_str) + strlen(header_str) + input_length + strlen(cmd.command) + 1) * sizeof(char));
    printf("here9\n");
    strcpy(msg, header_str);
    printf("msg +ch: %s\n", msg);
    strcat(msg, inphdr_str);
    printf("msg +ih: %s\n", msg);
    if(cmd.input!=NULL)
        strcat(msg, cmd.input);
    printf("msg +i: %s\n", msg);
    strcat(msg, cmd.command);
    printf("msg +c: %s\n", msg);
    printf("here4\n");
    printf("msg: %s\n", msg);
    // send the command message
    int num;
    num = write(cliex_fd, msg, strlen(msg));
    if (num < 0){
        perror ("write");
        printf ("Exiting application.\n");
        exit(1);
    }
    if (num < strlen(msg)){
        printf ("\nUnable to send the complete command to client. Possible network error. Exiting application.\n");
        exit(1);
    }
    printf("here5\n");
    // read the output header received as response
    char* output_hdr = malloc((1 + HEADER_SIZE) * sizeof(char));
    int bytes_read = 0;
    bytes_read = read (cliex_fd, output_hdr, HEADER_SIZE);
    if (bytes_read < 0){
        perror ("read");
        exit(1);
    }
    printf("output header: %s\n", output_hdr);
    printf("here6\n");
    // read the rest of the output received as response
    char* output = NULL;
    if (output_hdr[0] == 'o'){
        // setup the receiving char array
        output_hdr[HEADER_SIZE] = '\0';
        int output_size = atoi(output_hdr+1);
        output = malloc((output_size+1)*sizeof(char));

        // read from socket into output array
        bytes_read = read (cliex_fd, output, output_size);
        output[output_size] = '\0';
    }
    else {
        printf ("\nPossible application or network error detected. Exiting application.\n");
        exit(1);
    }

    free (output_hdr);
    close (cliex_fd);
    return output;
}

/*
    Accepts new clients, parses new commands, deploys commands to the respective machines
    then returns output to the requesting node.
*/
int main(int argc, char* argv[]){
    
    // read the config file into the nodelist structure
    NODE_LIST nodelist = get_nodelist_from_config (CONFIG_PATH);

    // create the main listening socket for the server
    int serv_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    // make address structure to bind the main listening server to the server port
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = SERV_PORT;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind the main listerning socket to the server port
    bind(serv_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    // start listening at the port
    listen(serv_socket, MAX_CONNECTION_REQUESTS_IN_QUEUE);

    // server setup
    CONNECTED_CLIENTS clients;
    clients.num = 0;

    // find total number of clients
    int num_of_clients = nodelist.num;
    printf ("num_of_clients: %d\n", num_of_clients);
    // accept connections from shell processes of all the clients and fill details in the client list structure
    for (int i = 0; i < num_of_clients; i++) {
        socklen_t sizereceived = sizeof(clients.list[clients.num].client_addr);
        if ((clients.list[clients.num].clientfd = accept(serv_socket, (struct sockaddr*)&clients.list[clients.num].client_addr, &sizereceived)) < 0 ) {
            perror("accept");
            exit(1);
        }
        printf ("Accepted connection from %s on socket number %d \n", inet_ntoa(clients.list[clients.num].client_addr.sin_addr), clients.list[clients.num].clientfd);
        clients.list[clients.num].nodenum = get_node_num(clients.list[clients.num].client_addr.sin_addr, nodelist);
        printf ("corresponding nodenum: %d\n", clients.list[clients.num].nodenum);
        clients.num++;
    }

    // listen for messages on each of the sockets and handle the commands received
    for(;;){
        

        // step 1: find the socket on which we have something to read (i.e. a command)
        fd_set rfds;
        FD_ZERO(&rfds);
        int max_fd = 0;
        // add the fds to the fd set rfds
        for (int i = 0; i < clients.num; i++){
            FD_SET(clients.list[i].clientfd, &rfds);
            if (clients.list[i].clientfd > max_fd)
                max_fd = clients.list[i].clientfd;
        }
        int retval = select(max_fd + 1, &rfds, NULL, NULL, NULL);
        if (retval == -1){
            perror("select()");
            exit(1);
        }
        char header_buf[HEADER_SIZE + 1];
        int commander_idx = 0;
        for (commander_idx = 0; commander_idx < clients.num; commander_idx++){
            if (FD_ISSET(clients.list[commander_idx].clientfd, &rfds))
                break;
        }
        printf ("Found available read on %s on socket number %d \n", inet_ntoa(clients.list[commander_idx].client_addr.sin_addr), clients.list[commander_idx].clientfd);
        int bytes_read = read (clients.list[commander_idx].clientfd, header_buf, HEADER_SIZE);
        printf ("header read, header[0]: %c\n", header_buf[0]);
        if (bytes_read < 0)
            perror ("read");
        if (bytes_read < HEADER_SIZE){ // bytes read is less than the minimum for any message (bytes in the header), probable cause network is network error
            printf ("\nPossible network error encountered. Exiting application.\n");
            return 0;
        }

        header_buf[HEADER_SIZE] = '\0'; // convert header to string
        printf ("command_idx: %d\n", commander_idx);
        printf ("command header read, header: %s\n", header_buf);
        printf ("address validation: %s\n", inet_ntoa(clients.list[commander_idx].client_addr.sin_addr));
        // step 2: handle the command
        if (header_buf[0] == 'c'){ // command received, according to message format
            
            int command_size = atoi(header_buf+1); //get the size of the message (command) as int
            printf ("command size read: %d\n", command_size);

            // read the command as string into command_buffer from the commander node socket
            char command[command_size+1]; 
            int bytes_read = read (clients.list[commander_idx].clientfd, command, command_size);
            command[command_size] = '\0';
            
            printf ("command read: %s\n", command);

            // parse the command
            PARSED_COMMANDS cmds = parse_command(command, clients.list[commander_idx].nodenum);
            
            // get the output to the command, either from server or by coordinating various nodes
            char* output = NULL;
            
            if (!strcmp(command, "nodes")){ // nodes command
                output = malloc((MAX_SIZE_OF_LINE_IN_CONFIG*MAX_NUM_OF_CONNECTED_CLIENTS + 1)*sizeof(char));
                output[0] = '\0';
                for(int i = 0; i < nodelist.num; i++){
                    int curr_size = strlen(output);
                    sprintf(output + curr_size, "n%d %s\n", i+1, nodelist.ip[i]);
                }
            }
            else {
            // execute the command on various nodes and get final output
                printf ("cmds.num: %d\n", cmds.num);
                for (int i = 0; i < cmds.num; i++){
                    cmds.list[i].input = output;
                    printf("here1\n");
                    output = execute_on_remote_node(cmds.list[i], clients); // output is the string containing the output of the command till subcommand i
                    printf("here2\n");
                }
                printf("here\n");
            }

            // send the final output to the commander node socket
            char* header_str = get_header_str("o", strlen(output));
            char* return_msg = malloc ((strlen(output) + strlen(header_str) + 1) * sizeof(char));
            strcpy(return_msg, header_str);
            strcat(return_msg, output);
            printf ("Sending message to commander: %s\n", return_msg);
            int bytes_sent = write (clients.list[commander_idx].clientfd, return_msg, strlen(return_msg));
            if (bytes_sent < 0){
                perror ("write");
                printf ("Exiting application.\n");
                return -1;
            }
            if (bytes_sent < strlen(return_msg)){
                printf ("\nUnable to send the complete output to client. Possible network error. Exiting application.\n");
                return -1;
            }
            // free memory
            free_parsed_commands (cmds);
            free (header_str);
            free (output);
            free (return_msg);
        }
        else { // message format not followed, 'c' not found as first letter
            printf ("\nPossible application or network error detected. Exiting application.\n");
            exit(1);
        }

    }
    return 0;
}

// testing
int testmain (){
    printf ("%s", get_header_str("i", 0));
}
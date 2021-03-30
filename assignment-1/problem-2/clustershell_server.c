/*
Clustershell server protocol:
1. Server is run on any one machine. All clients must specify the IP address of the server as a preprocessing directive in the client code (SERV_ADDRESS)
2. In the beginning all the clients listed in the config file connect to the server.
3. Upon recieving a command from any client, it connects to the specified machines and sends the required commands and inputs to those commands to those nodes.
4. Returns final output to the client that initially sent the command

Message Design:
Command messages from shell to server: 6 character command header + command
Command messages to from server to client: 6 character command header + 6 digit input header + input + command
Output messages: 6 character header + output 

The first letter of the header is c/o/i for command/output/input
The next 5 characters specify the length of the corresponding transmission

Message Design:
Command messages from shell to server: 6 character command header + command
Command messages to from server to client: 6 character command header + 6 digit input header + input + command
Output messages: 6 character header + output 

The first letter of the header is c/o/i for command/output/input
The next 5 characters specify the length of the corresponding transmission

Assumptions:
1. All clients listed in the config file connect in the beginning itself and none of them leave before all commands are over
2. There are no commands that require manual user input from the shell (stdin)
3. Commands, inputs and outputs are of maximum string length 99999 including all ending characters and newlines.
4. Nodes are named as n1, n2, n3, ... nN.
5. Nodes are listed in order in config file and no node number is missing
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
#define MAX_NUM_OF_CONNECTED_CLIENTS 30
// server port
#define SERV_PORT 12038
// this goes in the listen() call
#define MAX_CONNECTION_REQUESTS_IN_QUEUE 30
// size of header of messages (according to message format)
#define HEADER_SIZE 6
// maximum number of piped commands in a big main command sent by a node for distributed execution
#define MAX_NUMBER_OF_PIPED_COMMANDS 30
// max size of line in config file
#define MAX_SIZE_OF_LINE_IN_CONFIG 30
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

// path to config file
char* CONFIG_PATH = "config";


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
*/
char* execute_on_remote_node(COMMAND cmd, CONNECTED_CLIENTS clients) {
    // if the command of the n* kind, call execute_on_remote_node with modified command for all the clients and return the concatenated output
    if (cmd.nodenum == 0){
        char* output = malloc(sizeof(char));
        strcpy(output, "\0");
        for (int i = 0; i < clients.num; i++){
            cmd.nodenum = i+1;
            char* temp_output = execute_on_remote_node(cmd, clients);
            output = realloc (output, (strlen(output) + strlen(temp_output) + 1) * sizeof(char));
            strcat (output, temp_output);
        }
        cmd.nodenum = 0;
        return output;
    }
    
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
    // compose the command message
    char* header_str = get_header_str("c", strlen(cmd.command));
    char* inphdr_str = NULL;
    int input_length = 0;
    if (cmd.input != NULL)
        input_length = strlen(cmd.input);
    inphdr_str = get_header_str("i", input_length);
    char* msg = malloc((strlen(inphdr_str) + strlen(header_str) + input_length + strlen(cmd.command) + 1) * sizeof(char));
    strcpy(msg, header_str);
    strcat(msg, inphdr_str);
    if(cmd.input!=NULL)
        strcat(msg, cmd.input);
    strcat(msg, cmd.command);
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
    // read the output header received as response
    char* output_hdr = malloc((1 + HEADER_SIZE) * sizeof(char));
    int bytes_read = 0;
    bytes_read = read (cliex_fd, output_hdr, HEADER_SIZE);
    if (bytes_read < 0){
        perror ("read");
        exit(1);
    }
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
        printf ("\nPossible application or network error or client exit detected. Exiting application.\n");
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
    // get config file path from the user
    printf ("Please enter the path to the config file:\n");
    size_t n = 0;
    getline (&CONFIG_PATH, &n, stdin);
    n = strlen(CONFIG_PATH);
    CONFIG_PATH[n-1] = '\0';

    // read the config file into the nodelist structure
    NODE_LIST nodelist = get_nodelist_from_config (CONFIG_PATH);

    printf ("Config file successfully processed. Please proceed to connect all the clients listed in the config file\n");
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
    // accept connections from shell processes of all the clients and fill details in the client list structure
    for (int i = 0; i < num_of_clients; i++) {
        socklen_t sizereceived = sizeof(clients.list[clients.num].client_addr);
        if ((clients.list[clients.num].clientfd = accept(serv_socket, (struct sockaddr*)&clients.list[clients.num].client_addr, &sizereceived)) < 0 ) {
            perror("accept");
            exit(1);
        }
        printf ("Accepted connection from %s\n", inet_ntoa(clients.list[clients.num].client_addr.sin_addr));
        clients.list[clients.num].nodenum = get_node_num(clients.list[clients.num].client_addr.sin_addr, nodelist);
        clients.num++;
    }

    printf ("All clients successfully connected. The server is now handling commands.\n");
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
        int bytes_read = read (clients.list[commander_idx].clientfd, header_buf, HEADER_SIZE);
        if (bytes_read < 0)
            perror ("read");
        if (bytes_read < HEADER_SIZE){ // bytes read is less than the minimum for any message (bytes in the header), probable cause network is network error
            printf ("\nPossible network error encountered or client exit detected. Exiting application.\n");
            return 0;
        }

        header_buf[HEADER_SIZE] = '\0'; // convert header to string
        // step 2: handle the command
        if (header_buf[0] == 'c'){ // command received, according to message format
            
            int command_size = atoi(header_buf+1); //get the size of the message (command) as int

            // read the command as string into command_buffer from the commander node socket
            char command[command_size+1]; 
            int bytes_read = read (clients.list[commander_idx].clientfd, command, command_size);
            command[command_size] = '\0';
            

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
            else { // execute the command on various nodes and get final output
                for (int i = 0; i < cmds.num; i++){
                    cmds.list[i].input = output;
                    output = execute_on_remote_node(cmds.list[i], clients); // output is the string containing the output of the command till subcommand i
                }
            }

            // send the final output to the commander node socket
            char* header_str = get_header_str("o", strlen(output));
            char* return_msg = malloc ((strlen(output) + strlen(header_str) + 1) * sizeof(char));
            strcpy(return_msg, header_str);
            strcat(return_msg, output);
            int bytes_sent = write (clients.list[commander_idx].clientfd, return_msg, strlen(return_msg));
            if (bytes_sent < 0){
                perror ("write");
                printf ("Exiting application.\n");
                return -1;
            }
            if (bytes_sent < strlen(return_msg)){
                printf ("\nUnable to send the complete output to client. Possible network error or client exit. Exiting application.\n");
                return -1;
            }
            // free memory
            free_parsed_commands (cmds);
            free (header_str);
            free (output);
            free (return_msg);
        }
        else { // message format not followed, 'c' not found as first letter
            printf ("\nPossible application or network error or client exit detected. Exiting application.\n");
            exit(1);
        }

    }
    return 0;
}
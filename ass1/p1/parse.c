#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parse.h"
#include "constants.h"

PARSED_CMD* parse_cmd(char* cmd) {
    PARSED_CMD* new_cmd = (PARSED_CMD*) malloc(sizeof(PARSED_CMD));
    new_cmd->cmd_tokens = malloc(sizeof(char*) * MAX_NUM_CMDS);
    new_cmd->num_commands = 0;

    char* token = strtok(cmd, "|");
    int index = 0;
    if (token) {
        while (token) {
            new_cmd->cmd_tokens[index] = malloc(sizeof(char) * MAX_CMD_LEN);
            strcpy(new_cmd->cmd_tokens[index], token);

            token = strtok(NULL, "|");
            index++;
        }
        new_cmd->cmd_tokens[index] = NULL;
    }
    new_cmd->num_commands = index;

    // for (int i = 0; i < new_cmd->num_commands; i++) {
    //     printf("%s\n", new_cmd->cmd_tokens[i]);
    // }

    return new_cmd;
}

// int main() {
//     char cmd[] = "grep | ls | wc -a";
//     parse_cmd(cmd);

//     return 0;
// }
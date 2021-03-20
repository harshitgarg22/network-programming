#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parse.h"
#include "constants.h"

PARSED_CMD* parse_cmd(char* cmd) {
    PARSED_CMD* new_cmd = (PARSED_CMD*) malloc(sizeof(PARSED_CMD));
    new_cmd->cmd_tokens = malloc(sizeof(char*) * MAX_NUM_CMDS);
    new_cmd->num_tokens = 0;

    char* token = strtok(cmd, "|");
    int index = 0;
    if (token) {
        while (token) {
            new_cmd->cmd_tokens[index] = malloc(sizeof(char) * MAX_CMD_LEN);
            strcpy(new_cmd->cmd_tokens[index++], token);
            token = strtok(NULL, "|");

            if (token) {
                new_cmd->cmd_tokens[index] = malloc(sizeof(char));
                strcpy(new_cmd->cmd_tokens[index++], "|");
            }
        }
    }
    new_cmd->num_tokens = index;

    return new_cmd;
}

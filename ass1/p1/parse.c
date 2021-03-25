#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "constants.h"

PARSED_CMD parse_single_cmd(char* cmd) {
    PARSED_CMD new_cmd;
    new_cmd.cmd_tokens = malloc(sizeof(char*) * MAX_NUM_CMDS);
    new_cmd.num_tokens = 0;

    char* token = strtok(cmd, "|");
    int index = 0;
    if (token) {
        while (token) {
            new_cmd.cmd_tokens[index] = malloc(sizeof(char) * MAX_CMD_LEN);
            strcpy(new_cmd.cmd_tokens[index++], token);
            token = strtok(NULL, "|");

            if (token) {
                new_cmd.cmd_tokens[index] = malloc(sizeof(char));
                strcpy(new_cmd.cmd_tokens[index++], "|");
            }
        }
    }
    new_cmd.num_tokens = index;

    return new_cmd;
}

PARSED_CMD *parse_cmd(PARSED_CMD *cmdgrp, char *cmd) {
    char *double_pipe_loc = strstr(cmd, "||");
    char *triple_pipe_loc = strstr(cmd, "|||");

    char *parent = malloc(sizeof(char) * 10);
    if (double_pipe_loc) {
        strncpy(parent, cmd, double_pipe_loc - cmd);
        double_pipe_loc = double_pipe_loc + 2;

        char *children = strdup(double_pipe_loc);
        char *token = strtok(children, ",");
        
        int index = 0;
        while (token) {
            char *partial_cmd = malloc(sizeof(char) * 50);
            strcat(partial_cmd, parent);
            strcat(partial_cmd, "|");
            strcat(partial_cmd, token);

            token = strtok(NULL, ",");
            cmdgrp[index++] = parse_single_cmd(partial_cmd);
            free(partial_cmd);
        }
        cmdgrp[index].num_tokens = -1;
        free(children);
        free(parent);
    }
    else {
        cmdgrp[0] = parse_single_cmd(cmd);
        cmdgrp[1].num_tokens = -1;
    }
    return cmdgrp;
}

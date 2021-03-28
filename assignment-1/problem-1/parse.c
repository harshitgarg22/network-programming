#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "constants.h"

PARSED_SINGLE_CMD parse_single_cmd(char* cmd) {
    PARSED_SINGLE_CMD new_cmd;
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

PARSED_CMD parser(char *cmd) {
    char *double_pipe = strstr(cmd, "||");
    char *triple_pipe = strstr(cmd, "|||");

    PARSED_CMD parsed;

    if (double_pipe == NULL && triple_pipe == NULL) {
        parsed.pipe_type = 1;
        parsed.parent = strdup(cmd);
        parsed.children = NULL;
        return parsed;
    }

    char *delim;
    if (triple_pipe) {
        delim = "|||";   
        parsed.pipe_type = 3;
    }
    else if (double_pipe) {
        delim = "||";
        parsed.pipe_type = 2;
    }

    char *dup_cmd = strdup(cmd);
    char *token = strtok(dup_cmd,delim);
    parsed.parent = strdup(token);
    token = strtok(NULL, delim);

    // token now points to the comma-separated string of child commands
    char *children = strdup(token);
    free(dup_cmd);
    parsed.children = malloc(sizeof(char*) * MAX_NUM_CMDS);

    char *child_tok = strtok(children, ",");
    int index = 0;
    while (child_tok) {
        parsed.children[index++] = strdup(child_tok);
        child_tok = strtok(NULL, ",");
    } 
    free(children);
    return parsed;
}

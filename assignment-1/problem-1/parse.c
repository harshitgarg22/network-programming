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

PARSED_CMD parse_cmd(char *cmd) {
    PARSED_CMD parsed;
    parsed.num_cmds = 0;

    char *double_loc = strstr(cmd, "||");
    char *triple_loc = strstr(cmd, "|||");

    if (!double_loc && !triple_loc) {
        parsed.cmds = malloc(sizeof(char*));
        parsed.cmds[0] = strdup(cmd);
        parsed.pipe_type = 1;
        return parsed;
    }

    char *delim;
    int pipe_type;
    if (triple_loc) {
        delim = "|||";
        parsed.pipe_type = 3;
    }
    else if (double_loc) {
        delim = "||";
        parsed.pipe_type = 2;
    }

    char *dupcmd = strdup(cmd);
    char *token = strtok(dupcmd,delim);
    char *parent = strdup(token);
    token = strtok(NULL, delim);
    char *children = strdup(token);
    free(dupcmd);

    char *child = strtok(children, ",");
    parsed.cmds = malloc(sizeof(char*) * MAX_NUM_CMDS);
    int index = 0;

    while (child) {
        parsed.cmds[index] = malloc(sizeof(char) * MAX_CMD_LEN);
        strcat(parsed.cmds[index], parent);
        strcat(parsed.cmds[index], "|");
        strcat(parsed.cmds[index], child);
        index++;
        child = strtok(NULL, ",");
    }
    return parsed;
}

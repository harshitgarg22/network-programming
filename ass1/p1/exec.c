#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "exec.h"

int exec_cmd(char *cmd) {
    char *dup_cmd = strdup(cmd);
    char *dup_cmd2 = strdup(cmd);
    int num_args = 0;

    char *token = strtok(dup_cmd, " ");
    while (token) {
        num_args++;
        token = strtok(NULL, " ");
    }
    free(dup_cmd);

    char *cmd_args[num_args+1];
    char *token2 = strtok(dup_cmd2, " ");
    int i = 0;
    while (token2) {
        cmd_args[i++] = token2;
        token2 = strtok(NULL, " ");
    }
    cmd_args[i] = NULL;
    free(dup_cmd2);

    
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "exec.h"
#include "search_path.h"

int exec_cmd(char *cmd) {
    int num_args = count_args(cmd);
    printf("%d\n", num_args);
    char *dup_cmd = strdup(cmd);

    // vector of all arguments in the command (including command itself)
    // e.g. ls -a becomes ["ls", "-a"]
    char *cmd_args[num_args+1];  

    char *token = strtok(dup_cmd, " ");
    int i = 0;
    while (token) {
        printf("%s, ", token);
        cmd_args[i++] = strdup(token);
        token = strtok(NULL, " ");
    }
    cmd_args[i] = NULL;  // null-terminate for iteration
    free(dup_cmd);

    printf("%s\n", cmd_args[0]);
    char *canonical_path = search_path(cmd_args[0]);
    if (canonical_path == NULL) {
        fprintf(stderr, "%s: not found\n", cmd_args[0]);
        return 0;
    }

    if (execv(canonical_path, cmd_args) == -1) {
        fprintf(stderr, "Could not execute command\n");
        fprintf(stderr, "%s\n", strerror(errno));
    }
    free(canonical_path);
}

int count_args(char *cmd) {
    char *cmd_dup = strdup(cmd);
    char *token = strtok(cmd_dup, " ");
    int num_token = 0;

    while (token) {
        num_token++;
        token = strtok(NULL, " ");
    }
    free(cmd_dup);
    return num_token;
}

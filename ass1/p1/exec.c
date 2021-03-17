#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
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
        cmd_args[i++] = strdup(token);
        token = strtok(NULL, " ");
    }
    cmd_args[i] = NULL;  // null-terminate for iteration
    free(dup_cmd);

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

char *handle_redirection(char *cmd) {
    char *dup_cmd = strdup(cmd);
    char *token = strtok(dup_cmd, "<>");

    /*
    character pointed to by cmd+strlen(token) is either < or >
    if cmd+strlen(token)+1 is >, then it means the command contains >> operator
    so check for that first and then check for > or <
    */

    if (strcmp(token,dup_cmd) != 0) {
        // cmd contains redirection operators
        char *file;
        if (*(cmd + strlen(token) + 1) == '>') {
            // append to file
            file = cmd + strlen(token) + 2;
            int output_fd = open(file, O_CREAT | O_WRONLY | O_APPEND, S_IWUSR | S_IWGRP);
            if (output_fd == -1) {
                fprintf(stderr, "Could not open file for redirected append\n");
                fprintf(stderr, "%s\n", strerror(errno));
            }
            else {
                // TODO close write fd of output_fd
                close(1);  // close stdout
                dup(output_fd);  // duplicate output fd to redirect output
            }
            free(dup_cmd);
            return token;
        }
        else if (*(cmd + strlen(token)) == '>') {
            // write to file (clear any previous content)
            file = cmd + strlen(token) + 1;
            int output_fd = open(file, O_CREAT | O_WRONLY, S_IWUSR | S_IWGRP);
            if (output_fd == -1) {
                fprintf(stderr, "Could not open file for redirected output\n");
                fprintf(stderr, "%s\n", strerror(errno));
            }
            else {
                close(1);  // close stdout
                dup(output_fd);  // duplicate output fd to redirect output
            }
            free(dup_cmd);
            return token;
        }
        else if (*(cmd + strlen(token)) == '<') {
            // read from file
            file = cmd + strlen(token) + 1;
            int input_fd = open(file, O_CREAT | O_RDONLY, S_IRUSR | S_IRGRP | S_IROTH);
            if (input_fd == -1) {
                fprintf(stderr, "Could not open file for redirected input\n");
                fprintf(stderr, "%s\n", strerror(errno));
            }
            else {
                close(1);  // close stdout
                dup(input_fd);  // duplicate output fd to redirect output
            }
            free(dup_cmd);
            return token;
        }
    }
    return cmd;
}

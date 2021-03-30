#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "io_redirection.h"

char *remove_spaces(char *filename) {
    char *clean_name = malloc(sizeof(char) * strlen(filename));
    int j = 0;
    for (int i = 0; i < strlen(filename); i++) {
        if (filename[i] != ' ') {
            clean_name[j++] = filename[i];
        }
    }
    clean_name[j] = '\0';
    return clean_name;
}

char *handle_redirection(char *cmd) {
    char *dup_cmd = strdup(cmd);
    char *token = strtok(dup_cmd, "<>");

    if (strcmp(cmd, token) != 0) {
        // string contains either < or > or both
        char *file;
        if (strstr(cmd, ">>") != NULL) {
            file = remove_spaces(cmd + strlen(token) + 2);
            int fd;
            if ((fd = open(file, O_CREAT | O_WRONLY | O_APPEND, 0644)) < 0) {
                perror("Could not open file for append op\n");
                return NULL;
            }
            printf("appending output of '%s' to file '%s'\n", token, file);
            dup2(fd, 1);
            return token;
        }
        else if (strstr(cmd, ">") != NULL) {
            file = remove_spaces(cmd + strlen(token) + 1);
            int fd;
            if ((fd = open(file, O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0) {
                perror("Could not open file for write op\n");
                return NULL;
            }
            printf("writing output of '%s' to file '%s'\n", token, file);
            dup2(fd, 1);
            // free(dup_cmd);
            return token;
        }
        else if (strstr(cmd, "<") != NULL) {
            file = remove_spaces(cmd + strlen(token) + 1);
            int fd;
            if ((fd = open(file, O_CREAT | O_RDONLY, 0644)) < 0) {
                perror("Could not open file for read op\n");
                return NULL;
            }
            printf("reading input for '%s' from file '%s'\n", token, file);
            dup2(fd, 0);
            // free(dup_cmd);
            return token;
        }
    }
    free(dup_cmd);
    return cmd;
}
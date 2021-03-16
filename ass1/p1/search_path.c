#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdbool.h>
#include "constants.h"
#include "search_path.h"

char *search_path(char *cmd) {
    char *canonical_path = NULL;
    char *env = strdup(getenv(ENV));
    bool found = false;

    char *token = strtok(env, ":");
    while (token) {
        char *complete_path = malloc(sizeof(char) * (strlen(token) + MAX_CMD_LEN + 5));  // space alloted to store concatenated command name
        strcpy(complete_path, token);
        strcat(complete_path, "/");
        strcat(complete_path, cmd);

        // complete_path now stores a possible canonical path for the file
        // test whether this path exists in the file system or not
        struct stat statbuff;
        if (stat(complete_path, &statbuff) == -1) {
            if (errno != ENOENT) {
                fprintf(stderr, "!! ERROR: %s !!\n", strerror(errno));
                printf("Exiting...\n");
                exit(-1);
            }
        }
        else {
            // file exists because stat returns true value
            // check if the file is regular and if it has execute permissions
            if (S_ISREG(statbuff.st_mode) && (statbuff.st_mode && ( S_IXUSR | S_IXGRP | S_IXOTH ))) {
                found = true;
            }
        }

        if (found) {
            canonical_path = strdup(complete_path);
            free(complete_path);
            break;
        }
        free(complete_path);
        token = strtok(NULL, ":");  // repeat same process for next directory in PATH
    }
    return canonical_path;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "exec.h"
#include "search_path.h"
#include "constants.h"
#include "parse.h"
#include "io_redirection.h"
#include "colors.h"

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

int exec_single_cmd(char *cmd) {
    // vector of all arguments in the command (including command itself)
    // e.g. ls -a becomes ["ls", "-a"]
    int num_args = count_args(cmd);
    char *cmd_args[num_args+1];  

    char *dup_cmd = strdup(cmd);
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
        fprintf(stderr, "%s: command not found\n", cmd_args[0]);
        return -1;
    }

    if (execv(canonical_path, cmd_args) == -1) {
        fprintf(stderr, "Could not execute command\n");
        fprintf(stderr, "%s\n", strerror(errno));
    }
    free(canonical_path);
    return 0;
}

PROC_IPC *create_ipc_pipe(int read_fd, int write_fd) {
    PROC_IPC *new_pipe = (PROC_IPC*) malloc(sizeof(PROC_IPC));
    new_pipe->read_fd = read_fd;
    new_pipe->write_fd = write_fd;

    return new_pipe;
}

int READ_EXEC_WRITE(PROC_IPC *read_from, char *single_cmd, PROC_IPC *write_to) {
    int pid = getpid();
    printf("\nExecuting command ** %s **\n", single_cmd);
    printf("PID - %d\n", pid);
    printf("PGID - %d\n\n", getpgid(pid));

    if (read_from != NULL) {
        printf("Reading input from fd - %d\n", read_from->read_fd);
        close(read_from->write_fd);     // read end does not need to write anything
        dup2(read_from->read_fd, 0);    // close stdin and copy the read_end to stdin 
        fprintf(stderr, "STDIN has been remapped to fd %d\n\n", read_from->read_fd);
    }

    if (write_to != NULL) {
        printf("Writing output to fd - %d\n", write_to->write_fd);
        close(write_to->read_fd);       // write end does not need to read anything
        dup2(write_to->write_fd, 1);    // close stdout and copy the write_end to stdout  
        fprintf(stderr, "STDOUT has been remapped to fd %d\n\n", write_to->write_fd);
    }
    char *redirected_cmd = handle_redirection(single_cmd);
    return exec_single_cmd(redirected_cmd);
}

int exec_cmd(char *cmd) {
    PARSED_SINGLE_CMD parsed = parse_single_cmd(cmd);
    
    PROC_IPC *read_end = NULL;
    PROC_IPC *write_end = NULL;

    int i = 0;
    while (i < parsed.num_tokens) {
        char *command = parsed.cmd_tokens[i++];

        if (i > 1) {
            // first process does not need a read end but any process after that
            // will use the write_end of the previous process as its own read_end
            read_end = write_end;
            close(read_end->write_fd);
        }

        if (i < parsed.num_tokens) {
            if (strcmp(parsed.cmd_tokens[i++], "|") == 0) {
                // pipe operator after current command
                // this process needs to write its output to a pipe
                int p[2];
                pipe(p);
                write_end = create_ipc_pipe(p[0], p[1]);
            }
        }
        else write_end = NULL;

        // read and write pipes for the process are defined
        // start executing current sub-process
        pid_t child_proc = fork();
        if (child_proc < 0) {
            fprintf(stderr, "Could not spawn new child process for single command\n");
            exit(EXIT_FAILURE);
        }
        else if (child_proc == 0) {
            // inside child process
            if (READ_EXEC_WRITE(read_end, command, write_end) == 0) {
                exit(EXIT_SUCCESS);
            }
        }
        else {
            int child_proc_status;
            waitpid(child_proc, &child_proc_status, WUNTRACED);
            if (WIFEXITED(child_proc_status) || WIFSIGNALED(child_proc_status)) {
                printf("\nCommand done executing ...\n");
                // remove_proc(child_exec_proc);
            }
            if (WIFSTOPPED(child_proc_status)) {
                // child process was stopped by a signal
                printf("\nCommand stopped by signal %d\n", WSTOPSIG(child_proc_status));
                // set_proc_status(child_exec_proc, STOPPED);
            }
        }
    }
    exit(EXIT_SUCCESS);
}

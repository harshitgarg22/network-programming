#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

#include "constants.h"
#include "exec.h"
#include "colors.h"
// #include "exec_sc.h"
// #include "process_control.h"

int main(int argc, char* argv[]) {

    green();
    printf("\n\n============================================\n\n");
    printf("\t   << SHELL PROCESS >>\n");
    printf("\n\t PID:%d | PGID:%d", getpid(), getpgid(getpid()));
    printf("\n\n============================================\n");
    reset();
    // initalise lookup table
    // LKP_TABLE *sc_table = create_table();

    while (true) {
        cyan();
        printf("\n[shell]-> ");   // bash style prompt
        reset();
        char* cmd_buff = (char*)malloc(sizeof(char) * MAX_CMD_LEN);
        size_t cmd_len = MAX_CMD_LEN + 1;
        ssize_t cmd_inp_len = getline(&cmd_buff, &cmd_len, stdin);  // coz scanf is for noobs, you noob

        if (cmd_inp_len <= 0 ||  (cmd_inp_len == 1 && cmd_buff[0] == '\n')) {
            continue;
        }
        cmd_buff[cmd_inp_len-1] = 0;
        if (strcmp(cmd_buff, "exit") == 0) {
            green();
            printf("\nEXITING TERMINAL...\n\n");
            reset();
            free(cmd_buff);
            break;
        }

        // check for background process
        bool is_bg_process = false;
        if (cmd_buff[cmd_inp_len-2] == '&') {
            is_bg_process = true;
        }

        // create child process start execution of command in new process group
        pid_t child_exec_proc = fork();

        if (child_exec_proc < 0) {
            fprintf(stderr, "Error spawning new process group for this command. Exiting...\n\n");
            exit(1);
        }
        else if (child_exec_proc == 0) {
            // inside child process
            int curr_pid = getpid();
            printf("Starting execution of new command\n");
            printf("\tProcess ID : %d\n", curr_pid);
            printf("\tProcess Grp ID : %d\n", getpgid(curr_pid));
            printf("\n");

            ///////////////////////////////////////////////////////////////////////////
            exec_cmd(cmd_buff);
            ///////////////////////////////////////////////////////////////////////////
        }
        else {
            // set process group id to curr_pid
            if (setpgid(child_exec_proc, child_exec_proc) == -1) {
                fprintf(stderr, "Could not set current process group for foreground execution\n");
                exit(0);
            }

            if (!is_bg_process) {
                // set disposition of SIGTTOU to ignore so that parent can still print output to terminal
                signal(SIGTTOU, SIG_IGN);
                //set the child pid as the foreground process group id and the controlling terminal
                if (tcsetpgrp(STDIN_FILENO, child_exec_proc) == -1) {
                    fprintf(stderr, "Unable to bring process grp to foreground. Exiting...\n\n");
                    exit(0);
                }
                else {
                    printf("controlling terminal is now %d\n", tcgetpgrp(0));
                }
            }

            // command *cmd = malloc(sizeof(command));
            // cmd->cmd = cmd_buff;
            // cmd->is_bg = is_bg_process;
            // cmd->pgid = child_exec_proc;

            // add_proc(cmd);
            int child_proc_status;
            if (!is_bg_process) {
                // wait for the process to either finish execution or terminate
                while (1) {
                    waitpid(child_exec_proc, &child_proc_status, WUNTRACED);
                    if (WIFEXITED(child_proc_status) || WIFSIGNALED(child_proc_status)) {
                        printf("\n\nProcess done executing-----------\n\n");
                        // remove_proc(child_exec_proc);
                        break;
                    }
                    if (WIFSTOPPED(child_proc_status)) {
                        // child process was stopped by a signal
                        printf("\n\nProcess stopped by signal %d\n", WSTOPSIG(child_proc_status));
                        // set_proc_status(child_exec_proc, STOPPED);
                        break;
                    }
                }

                // give control back to the shell process
                tcsetpgrp(STDIN_FILENO, getpid());
                printf("returning controll to shell process\n");
                printf("controlling terminal is now - %d\n", tcgetpgrp(0));
                // reset disposition to default
                signal(SIGTTOU, SIG_DFL);
            }
        }
        // printf("here\n");
    }
    return 0;
}
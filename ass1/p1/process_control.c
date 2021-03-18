#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "process_control.h"

int add_proc(command *cmd) {
    for (int i = 0; i < MAX_NUM_CMDS_ALLOWED; i++) {
        if (curr_commands[i] == NULL) {
            curr_commands[i] = cmd;
            curr_commands[i]->cmd = strdup(curr_commands[i]->cmd);
            cmd->proc_id = i;
            return i;
        }
    }
    return -1;
}

int remove_proc(int pgid) {
    int proc_id = -1;
    for (int i = 0; i < MAX_NUM_CMDS_ALLOWED; i++) {
        if (curr_commands[i]->pgid == pgid) {
            proc_id = i;
            break;
        }
    }
    if (curr_commands[proc_id] == NULL) {
        return -1;
    }
    free(curr_commands[proc_id]->cmd);
    free(curr_commands[proc_id]);
    curr_commands[proc_id] = NULL;
    return 0;
}

int set_proc_status(int pgid, enum PROCESS_STATE new_state) {
    int proc_id = -1;
    for (int i = 0; i < MAX_NUM_CMDS_ALLOWED; i++) {
        if (curr_commands[i]->pgid == pgid) {
            proc_id = i;
            break;
        }
    }
    if (curr_commands[proc_id] == NULL) {
        return -1;
    }
    curr_commands[proc_id]->is_bg = true;
    curr_commands[proc_id]->proc_state = new_state;
}

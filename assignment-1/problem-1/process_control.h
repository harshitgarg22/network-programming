
/*
These functions are to manage the status of processes and to maintain a 
record of the processes that are currently executing as children of the shell process
*/

#include <stdbool.h>
#include <unistd.h>
#include "constants.h"

enum PROCESS_STATE { RUNNING, STOPPED };

typedef struct executing_cmd {
    int proc_id;
    char *cmd;
    bool is_bg;
    pid_t pgid;
    enum PROCESS_STATE proc_state;
} command;

extern command *curr_commands[MAX_NUM_CMDS_ALLOWED];

// add this command to the list of running processes and
// set the proc_id of cmd to the index at which it was 
// added to the list
int add_proc(command *cmd);

// remove command belonging to group pgid from 
// currently executing processes
int remove_proc(int pgid);

// set state of running process to new_state
int set_proc_status(int pgid, enum PROCESS_STATE new_state);

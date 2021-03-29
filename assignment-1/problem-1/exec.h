// Struct representing the read and write fd's of a subprocess.
// Every subprocess has its own struct through which it reads
// from other processes (or stdin) and pipes output to other 
// processes (or stdout)
typedef struct ipc_unit {
    int read_fd;
    int write_fd;
} PROC_IPC;

PROC_IPC *create_ipc_pipe(int read_fd, int write_fd);

// Parse a single command by separating its arguments.
// Execute the command and return 0 if execution is not possible
int exec_single_cmd(char *cmd);

// Count number of tokens in single command
int count_args(char *cmd);

// Handle redirection operators <, > and >>
char *handle_redirection(char *cmd);

// Execute a complete process group. One group may contain a 
// sequence of commands or just a single command.
int exec_cmd(char *cmd);

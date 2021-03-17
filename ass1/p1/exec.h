// parse a single command by separating its arguments
// execute the command, return 0 if execution not possible
int exec_single_cmd(char *cmd);

//count number of tokens in single command
int count_args(char *cmd);

// handle redirection operators <, > and >>
char *handle_redirection(char *cmd);

int exec(char *cmd);
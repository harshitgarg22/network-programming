typedef struct parsed_cmd {
    char **cmd_options;  // array of strings of commands (after removing pipes)
    int num_commands;  // number of commands
    int is_bg;  // whether the command is meant to run in background or foreground
} PARSED_CMD;

PARSED_CMD* parse_cmd(char* cmd_inp);

typedef struct parsed_cmd {
    char **cmd_options;  // array of arrays of strings of commands (after removing pipes)
    int num_commands;  // number of commands
} PARSED_CMD;

PARSED_CMD* parse_cmd(char* cmd_inp);

typedef struct parsed_cmd {
    char **cmd_tokens;  // array of strings of commands (after removing pipes)
    int num_tokens;  // number of commands
} PARSED_CMD;

PARSED_CMD* parse_cmd(char* cmd_inp);

typedef struct parsed_cmd {
    char **cmds;
    int num_cmds;
    int pipe_type;
} PARSED_CMD;

typedef struct parsed_single_cmd {
    char **cmd_tokens;  // array of strings of commands (after removing pipes)
    int num_tokens;  // number of commands
} PARSED_SINGLE_CMD;

PARSED_CMD parse_cmd(char *cmd);

PARSED_SINGLE_CMD parse_single_cmd(char *cmd);

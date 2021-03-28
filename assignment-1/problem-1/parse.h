typedef struct parsed_single_cmd {
    char **cmd_tokens;  // array of strings of commands (after removing pipes)
    int num_tokens;  // number of commands
} PARSED_SINGLE_CMD;

typedef struct parsed_cmd {
    char *parent;  // parent command (string before || or |||)
    char **children; // array of child commands (separated by ',' in cmd)
    int pipe_type;  // 1(with only simple pipes), 2 (with ||), 3 (with |||)
} PARSED_CMD;

PARSED_SINGLE_CMD parse_single_cmd(char* cmd);

PARSED_CMD parse_cmd(char *cmd);

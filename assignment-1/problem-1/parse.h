typedef struct parsed_cmd {
    char **cmds;
    int num_cmds;
    int pipe_type;
} PARSED_CMD;

typedef struct parsed_single_cmd {
    char **cmd_tokens;  // array of strings of commands (after removing pipes)
    int num_tokens;  // number of commands
} PARSED_SINGLE_CMD;

// parses a complete command and breaks it up into strings separated by special pipe operators
// ls || sort, wc becomes ["ls | sort", "ls | wc"]
PARSED_CMD parse_cmd(char *cmd);

// parses a single command and splits it into tokens
// "ls | sort" becomes ["ls","|","sort"]
PARSED_SINGLE_CMD parse_single_cmd(char *cmd);

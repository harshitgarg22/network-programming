#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "constants.h"

int main(int argc, char* argv[]) {
    printf("New shell created!");
    while (true) {
        printf("$ ");   // bash style prompt
        char* cmd_buff = (char*)malloc(sizeof(char) * MAX_CMD_LEN);
        size_t cmd_len = MAX_CMD_LEN + 1;
        ssize_t cmd_inp_len = getline(&cmd_buff, &cmd_len, stdin);  // coz scanf is for noobs, you noob

        if (cmd_inp_len <= 0 ||  (cmd_inp_len == 1 && cmd_buff[0] == '\n')) {
            continue;
        }

        
    }
}
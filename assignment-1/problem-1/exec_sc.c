#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "exec_sc.h"

extern LKP_TABLE *sc_table;

LKP_TABLE *create_table() {
    LKP_TABLE *new_table = (LKP_TABLE*)malloc(sizeof(LKP_TABLE));

    new_table->num_entries = 0;
    new_table->table = malloc(sizeof(TB_ENTRY) * MAX_LOOKUP_TABLE_SIZE);
    return new_table;
}

int insert_entry(int key, char* cmd) {
    if (sc_table->table[key].assigned) {
        fprintf(stderr, "\nA command has already been assigned to this key. Cannot overwrite.\n");
        return -1;
    }
    sc_table->table[key].assigned = true;
    sc_table->table[key].key = key;
    sc_table->table[key].cmd = (char *) malloc(sizeof(char) * strlen(cmd));
    strcpy(sc_table->table[key].cmd, cmd);
    
    printf("\nAdding entry to lookup table:\n");
    printf("Key: %d | Command: %s\n", key, cmd);
    printf("Done...\n");

    sc_table->num_entries++;

    return key;
}

int delete_entry(int key) {
    if (!sc_table->table[key].assigned) {
        fprintf(stderr, "No entry has been assigned to this key. Cannot delete non-existent entry.\n");
        return -1;
    }
    sc_table->table[key].assigned = false;
    free(sc_table->table[key].cmd);
    
    printf("\nDeleting entry for key %d from lookup table\n", key);
    sc_table->num_entries--;
    
    return key;
}

int exec_sc(char *cmd, LKP_TABLE *sc_table) {
    char *dup_cmd = strdup(cmd);
    char *token = strtok(dup_cmd, " ");

    char *options[4];    // 4 args in sc command - sc, i/d, index, cmd
    char *pointer = cmd;
    for (int i = 0; i < 3; i++) {
        options[i] = strdup(token);
        pointer += strlen(token) + 1;
        token = strtok(NULL, " ");
    }
    options[3] = strdup(pointer);
    free(dup_cmd);

    if (strcmp(options[1], "-i") == 0) {
        return insert_entry(atoi(options[2]), options[3]);
    }
    else if (strcmp(options[1], "-d") == 0) {
        return delete_entry(atoi(options[2]));
    }
    return -1;
}

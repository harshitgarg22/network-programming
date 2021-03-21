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

    // very pretty hard-coded stuff; very bad;
    char *options[4];    // 3 options in sc command - i/d, index, cmd
    for (int i = 0; i < 4; i++) {
        if (i == 3) {
            // command string
            options[i] = strdup(cmd+8);
            break;
        }
        options[i] = strdup(token);
        token = strtok(NULL, " ");
    }
    free(dup_cmd);

    if (strcmp(options[1], "-i") == 0) {
        // insert in lookup table
        if (insert_entry(atoi(options[2]), options[3]) == -1) {
            return -1;
        }
    }
    else if (strcmp(options[1], "-d") == 0) {
        // delete from lookup table
        if (delete_entry(atoi(options[2])) == -1) {
            return -1;
        }
    }
    return atoi(options[2]);
}
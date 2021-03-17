#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "constants.h"
#include "exec_sc.h"


LKP_TABLE *create_table() {
    LKP_TABLE *new_table = malloc(sizeof(LKP_TABLE));

    new_table->num_entries = 0;
    new_table->table = malloc(sizeof(TB_ENTRY) * MAX_LOOKUP_TABLE_SIZE);
    return new_table;
}

int insert_entry(LKP_TABLE* sc_table, int key, char* cmd) {
    if (sc_table->table[key].assigned) {
        fprintf(stderr, "A command has already been assigned to this key. Cannot overwrite.\n");
        return -1;
    }
    sc_table->table[key].assigned = true;
    sc_table->table[key].key = key;
    sc_table->table[key].cmd = (char *) malloc(sizeof(char) * strlen(cmd));
    strcpy(sc_table->table[key].cmd, cmd);

    return key;
}

int delete_entry(LKP_TABLE* sc_table, int key) {
    if (!sc_table->table[key].assigned) {
        fprintf(stderr, "No entry has been assigned to this key. Cannot delete what doesn't exist lol\n\n");
        return -1;
    }
    sc_table->table[key].assigned = false;
    free(sc_table->table[key].cmd);
    return key;
}

int main() {
    LKP_TABLE* sc_table = create_table();
    if (insert_entry(sc_table, 1, "ls -al") == -1) {
        printf("error\n");
    }
    printf("inserted %d -> %s\n", sc_table->table[1].key, sc_table->table[1].cmd);
    if (insert_entry(sc_table, 2, "grep") == -1) {
        printf("error\n");
    }
    printf("inserted %d -> %s\n", sc_table->table[2].key, sc_table->table[2].cmd);

    insert_entry(sc_table, 1, "wc");

    if (delete_entry(sc_table, 1) != -1) {
        printf("deleted entry at %d\n", sc_table->table[1].key);
    }

    insert_entry(sc_table, 1, "wc");
    printf("inserted %d -> %s\n", sc_table->table[1].key, sc_table->table[1].cmd);

    if (delete_entry(sc_table, 2) != -1) {
        printf("deleted entry at %d\n", sc_table->table[2].key);
    }
    if (delete_entry(sc_table, 2) != -1) {
        printf("deleted entry at %d\n", sc_table->table[2].key);
    }
    return 0;
}

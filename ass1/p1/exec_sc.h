#include <stdbool.h>

typedef struct lookup_table_entry {
    int key;
    char* cmd; 
    bool assigned;
} TB_ENTRY;

typedef struct lookup_table {
    TB_ENTRY *table;   // vector of all entries
    int num_entries;    // number of assigned entries 
} LKP_TABLE;

// create global lookup table
LKP_TABLE* create_table();

// insert entry into table for this key and assign command
// return key if successful, -1 if not
int insert_entry(int key, char *cmd);

// delete entry for this key
// return key if successful, -1 if not
int delete_entry(int key);

// execute the sc command
int exec_sc(char *cmd, LKP_TABLE *sc_table);

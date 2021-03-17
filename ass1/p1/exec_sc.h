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

// check if table contains entry for this key
// return 1 if yes, 0 if not
bool has_entry(LKP_TABLE* sc_table, int key);

// insert entry into table for this key and assign command
// return key if successful, -1 if not
int insert_entry(LKP_TABLE* sc_table, int key, char *cmd);

// delete entry for this key
// return key if successful, -1 if not
int delete_entry(LKP_TABLE* sc_table, int key);

// execute the sc command
int exec_sc(char *cmd, LKP_TABLE *sc_table)
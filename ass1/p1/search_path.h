#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdbool.h>
#include "constants.h"

// search for command file in PATH variable
// return NULL if command not found
char *search_path(char *cmd);
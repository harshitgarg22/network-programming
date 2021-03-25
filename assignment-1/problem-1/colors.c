#include <stdio.h>
#include "colors.h"

void red () {
  printf("\033[1;31m");
}

void green () {
  printf("\033[1;32m");
}

void cyan () {
  printf("\033[1;36m");
}

void reset () {
  printf("\033[0m");
}
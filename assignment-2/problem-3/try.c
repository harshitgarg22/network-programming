#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

typedef struct sockaddr sa;

typedef struct msg {
    void *buff;
} mg;

#define _ADDR_ sizeof(struct sockaddr_in)

int main() {
    printf("%ld\n", _ADDR_);
    return 0;
}
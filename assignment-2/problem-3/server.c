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

int main() {
    int f = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    int l = sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(12345);
    bind(f, (sa*)&addr, l);
    mg newmsg;
    int n = recvfrom(f, &newmsg, 20, 0, (sa*)&addr, &l);
    printf("%s\n", newmsg.buff);

    close(f);
    return 0;
}
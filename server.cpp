#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <queue>
#include <vector>
#include "threadpool.h"

void do_close(void *arg) {
    int fd = *((int *)arg);
}

int main(int argc, char **argv) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(19920);
    server.sin_addr.in_addr = INADDR_ANY;
    bind(sock, (struct sockaddr *)&server, sizeof(server));
    listen(sock, 1000);
    threadpool_t * threadpool = threadpool_create(1, 1000, 0);
    while (true) {
        int conn = accpet(sock, NULL, NULL);
        int *fd = new int;
        *fd = conn;
        threadpool_add(threadpool, do_close, (void *)fd, 0);
    }

    return 0;
}

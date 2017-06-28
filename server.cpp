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
using namespace std;

const char *msg = "nice weather huh!";

void do_close(void *arg) {
    int fd = *((int *)arg);
    char buffer[256];
    int count = read(fd, buffer, 256);
    buffer[count] = 0;
    cout<<"fd: "<<fd<<" count: "<<count<<"\treceive \""<<buffer<<"\""<<endl;
    write(fd, msg, strlen(msg));
    //close(fd);
    //cout<<"close fd: "<<fd<<endl;
    delete (int *)arg;
}

int main(int argc, char **argv) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(19920);
    server.sin_addr.s_addr = INADDR_ANY;
    int on = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));
    bind(sock, (struct sockaddr *)&server, sizeof(server));
    listen(sock, 1000);
    threadpool_t * threadpool = threadpool_create(1, 1000, 0);
    while (true) {
        int conn = accept(sock, NULL, NULL);
        cout<<"accept a new connection: "<<conn<<endl;
        int *fd = new int;
        *fd = conn;
        threadpool_add(threadpool, do_close, (void *)fd, 0);
    }

    return 0;
}

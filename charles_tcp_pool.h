#ifndef _CHARLES_TCP_POOL_H
#define _CHARLES_TCP_POOL_H
/**
 * Description: Charles self auto repair tcp pool
 * File: charles_tcp_pool.h
 * Author: Charles,Liu. 2017-6-27
 * Mailto: charlesliu.cn.bj@gmail.com
 */
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
using std::string;
using std::queue;
using std::vector;

/* retry count when connection is lost */
#define RETRY_COUNT 3
/* retry period when connection is lost */
#define RETRY_PERIOD 1 /* second */
/* thread pool size */
#define THREADPOOL_SIZE 2
/* thread pool queue size */
#define THREADPOOL_QUEUE_SIZE 1000
/* init connection pool size */
#define INIT_POOL_SIZE 3
/* max connection pool size */
#define MAX_POOL_SIZE 10

#define IPLEN 32

#define charles_err(fmt, ...) do {\
    fprintf(stderr, fmt, ##__VA_ARGS__);\
    fprintf(stderr, "\n");\
} while(0)

typedef struct tcp_connection_t {
    int fd;
    void *extra;
} tcp_connection_t;

class CharlesTcpPool {
public:
    CharlesTcpPool(char *ip, int port, int max_size = MAX_POOL_SIZE, int init_size = INIT_POOL_SIZE);
    ~CharlesTcpPool();
    int initPool(bool nonblock = false);
    tcp_connection_t * newConnection(bool nonblock = false);
    getConnection();
    putConnection();
public:
    void watchPool();
private:
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int max_pool_size;
    int init_pool_size;
    int pool_size;
    vector<tcp_connection_t *> pool;
    queue<tcp_connection_t *> ready_pool;
    threadpool_t *threadpool;
    int epollfd;
    char ip[IPLEN];
    int port;
};

#include "charles_tcp_pool.h"

int charles_epoll_create() {
    int epfd = epoll_create1(0);
    if (epfd == -1) {
        charles_err("epoll create failed (%s)", strerror(errno));
    }
    return epfd;
}

int charles_socket(int domain, int type, int protocol) {
    int ret = socket(domain, type, protocol);
    if (ret == -1) {
        charles_err("create socket failed (%s)", strerror(errno));
    }
    return ret;
}

int charles_inet_aton(const char *cp, struct in_addr *inp) {
    int ret = inet_aton(cp, inp);
    if (ret == 0) {
        charles_err("inet_aton: not valid server address (%s)", strerror(errno));
    }
    return ret;
}

int charles_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    int ret = connect(sockfd, addr, addrlen);
    if (ret == 1) {
        charles_err("connect failed (%s)", strerror(errno));
    }
    return ret;
}

int set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        charles_err("fcntl getfl failed (%s)", strerror(errno));
        return -1;
    }
    flags |= O_NONBLOCK;
    int ret = fcntl(fd, F_SETFL, flags);
    if (ret == -1) {
        charles_error("fcntl setlf failed (%s)", strerror(errno));
        return -1;
    }

    return 0;
}

/* call back into class */
void *watch_pool(void *arg) {
    CharlesTcpPool *pool = (CharlesTcpPool *)arg;
    pool->watchPool();
}

CharlesTcpPool::CharlesTcpPool(char *server_ip, int server_port, int max_size, int init_size) {
    strncpy(ip, server, IPLEN);
    port = server_port;
    max_pool_size = max_size;
    init_pool_size = init_size;
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    pool.reserve(max_size);
    pool_size = 0;
}

CharlesTcpPool::~CharlesTcpPool() {
    // do clean
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < pool.size(); ++i) {
        tcp_connection_t *connection = pool[i];
        if (connection->fd != -1)
            close(connection->fd);
        delete connection;
    }
    ready_queue = queue<tcp_connection_t *>();
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
}

tcp_connection_t * CharlesTcpPool::newConnection(bool nonblock) {
    tcp_connection_t *connection = new tcp_connection_t;
    connection->fd = charles_socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    charles_inet_aton(ip, &server.sin_addr);
    if (nonblock && -1 == set_nonblock(connection->fd)) {
        return NULL;
    }
    if (-1 == charles_connect(connection->fd, (struct sockaddr *)&server, sizeof(server))) {
        return NULL;
    }

    return connection;
}

int CharlesTcpPool::initPool(bool nonblock) {
    epollfd = charles_epoll_create();
    if (epollfd == -1)
        return -1;
    threadpool = threadpool_create(THREADPOOL_SIZE, THREADPOOL_QUEUE_SIZE, 0);
    if (threadpool == NULL)
        return -1;

    for (int i = 0; i < init_pool_size; ++i) {
        tcp_connection_t *connection = newConnection(nonblock);
        if (connection == NULL) /* fail if can't create connection */
            return -1;
        pool.push_back(connection);
        ready_pool.push(connection);
    }

    pthread_t watcher;
    pthread_attr_t watcher_attr;
    pthread_attr_init(&watcher_attr);
    pthread_attr_setdetachstate(&watcher_attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&watcher, &watcher_attr, watch_pool, (void *)this);
    pthread_attr_destroy(&watcher_attr);

    return 0;
}

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
    if (ret == -1) {
        charles_err("connect failed (%s)", strerror(errno));
    }
    return ret;
}

int charles_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    int ret = epoll_ctl(epfd, op, fd, event);
    if (ret == -1) {
        charles_err("epoll ctl failed (%s)", strerror(errno));
    }
    return ret;
}

int charles_epoll_wait(int epfd, struct epoll_event *events, int maxevents) {
    int ret = epoll_wait(epfd, events, maxevents, -1);
    if (ret == -1) {
        charles_err("epoll wait failed (%s)", strerror(errno));
    }
    return ret;
}

int set_nonblock(int fd, bool on) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        charles_err("fcntl getfl failed (%s)", strerror(errno));
        return -1;
    }
    if (on)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    int ret = fcntl(fd, F_SETFL, flags);
    if (ret == -1) {
        charles_err("fcntl setlf failed (%s)", strerror(errno));
        return -1;
    }

    return 0;
}

int set_tcp_nodelay(int sock, bool on) {
    int flag;
    if (on) 
        flag = 1;
    else
        flag = 0;
    int ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
    if (ret == -1) {
        charles_err("setsockopt failed (%s)", strerror(errno));
    }

    return ret;
}

int set_tcp_keepalive(int sock, bool on) {
    int flag;
    if (on)
        flag = 1;
    else 
        flag = 0;
    int ret = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
    if (ret == -1) {
        charles_err("setsockopt failed (%s), strerror(errno)");
    }

    return ret;
}

int get_socket_read_buffer_length(int fd) {
    int length = 0;
    int ret = ioctl(fd, FIONREAD, &length);
    if (ret == -1) {
        charles_err("get socket read buffer length failed (%s)", strerror(errno));
        return ret;
    }

    return length;
}

/* call back into class */
void *watch_pool(void *arg) {
    CharlesTcpPool *pool = (CharlesTcpPool *)arg;
    pool->watchPool();
}

/* call back into class */
void repair_connection(void *arg) {
    tcp_connection_t *connection = (tcp_connection_t *)arg;
    CharlesTcpPool *pool = (CharlesTcpPool *)connection->extra;
    pool->repairConnection(connection);
}

CharlesTcpPool::CharlesTcpPool(const char *server_ip, int server_port, int max_size, int init_size) {
    strncpy(ip, server_ip, IPLEN);
    port = server_port;
    max_pool_size = max_size;
    init_pool_size = init_size;
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    pool.reserve(max_size);
}

CharlesTcpPool::~CharlesTcpPool() {
    // do clean
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < pool.size(); ++i) {
        tcp_connection_t *connection = pool[i];
        pthread_rwlock_wrlock(&connection->rwlock);
        charles_epoll_ctl(epollfd, EPOLL_CTL_DEL, connection->fd, NULL);
        close(connection->fd);
        connection->valid = false;
        pthread_rwlock_unlock(&connection->rwlock);
        pthread_rwlock_destroy(&connection->rwlock);
        delete connection;
    }
    running = false;
    close(epollfd);
    ready_pool = queue<tcp_connection_t *>();
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
}

tcp_connection_t * CharlesTcpPool::newConnection(int flags) {
    tcp_connection_t *connection = new tcp_connection_t;
    connection->fd = charles_socket(AF_INET, SOCK_STREAM, 0);
    connection->flags = flags;
    connection->valid = true;
    connection->extra = (void *)this;
    pthread_rwlock_init(&connection->rwlock, NULL);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    charles_inet_aton(ip, &server.sin_addr);
    if (-1 == setConfig(connection->fd, flags)) {
        close(connection->fd);
        delete connection;
        return NULL;
    }
    if (-1 == charles_connect(connection->fd, (struct sockaddr *)&server, sizeof(server))) {
        close(connection->fd);
        delete connection;
        return NULL;
    }

    return connection;
}

int CharlesTcpPool::initPool(int flags) {
    epollfd = charles_epoll_create();
    if (epollfd == -1)
        return -1;
    threadpool = threadpool_create(THREADPOOL_SIZE, THREADPOOL_QUEUE_SIZE, 0);
    if (threadpool == NULL)
        return -1;

    for (int i = 0; i < init_pool_size; ++i) {
        tcp_connection_t *connection = newConnection(flags);
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

void CharlesTcpPool::watchPool() {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < init_pool_size; ++i) {
        struct epoll_event event;
        if (pool[i]->flags & CHARLES_SOCK_NONBLOCK)
            event.events = EPOLLIN | EPOLLET;
        else
            event.events = EPOLLIN;
        event.data.ptr = (void *)pool[i];
        charles_epoll_ctl(epollfd, EPOLL_CTL_ADD, pool[i]->fd, &event);
    }
    pthread_mutex_unlock(&mutex);
    running = true;
    struct epoll_event events[max_pool_size];
    while (running) {
        int nfds = charles_epoll_wait(epollfd, events, max_pool_size);
        if (nfds == -1)
            continue;
        tcp_connection_t *connection;
        for (int i = 0; i < nfds; ++i) {
            connection = (tcp_connection_t *)events[i].data.ptr;
            if (events[i].events & EPOLLIN) {
                if (0 == get_socket_read_buffer_length(connection->fd)) {
                    pthread_rwlock_wrlock(&connection->rwlock);
                    connection->valid = false;
                    pthread_rwlock_unlock(&connection->rwlock);
                    /* remove from epoll event */
                    charles_epoll_ctl(epollfd, EPOLL_CTL_DEL, connection->fd, NULL);
                    threadpool_add(threadpool, repair_connection, (void *)connection, 0);
                }
            }
        }
    }
}   

void CharlesTcpPool::repairConnection(tcp_connection_t *connection) {
    /* I must backup this fd, can't close it, it must remains a valid file descriptor */
    int backup = connection->fd;
    int count = 0;
    for (; count < RETRY_COUNT; ++count) {
        connection->fd = charles_socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        charles_inet_aton(ip, &server.sin_addr);
        if (-1 == setConfig(connection->fd, connection->flags)) {
            close(connection->fd);
            usleep(RETRY_PERIOD * 1000);
            continue;
        }
        if (-1 == charles_connect(connection->fd, (struct sockaddr *)&server, sizeof(server))) {
            close(connection->fd);
            usleep(RETRY_PERIOD * 1000);
            continue;
        }
        /* get a good connection */
        pthread_rwlock_wrlock(&connection->rwlock);
        connection->valid = true;
        close(backup); /* close backup if we new connection successfully */
        pthread_rwlock_unlock(&connection->rwlock);
        break;
    }
    /* add to epoll event even if this connection is not repaired, wait for next time repair */
    struct epoll_event event;
    if (connection->flags & CHARLES_SOCK_NONBLOCK)
        event.events = EPOLLIN | EPOLLET;
    else
        event.events = EPOLLIN;
    if (count == RETRY_COUNT) { /* failed, add original fd to epoll events */
        connection->fd = backup;
    }
    event.data.ptr = connection;
    charles_epoll_ctl(epollfd, EPOLL_CTL_ADD, connection->fd, &event);
}

tcp_connection_t *CharlesTcpPool::getConnection(int timeout /* milisecond */, int flags) {
    struct timespec ts;
    ts.tv_sec = timeout / 1000;
    ts.tv_nsec = (timeout % 1000) * 1000 * 1000;
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &start_time);
    pthread_mutex_lock(&mutex);
    while (ready_pool.empty() && pool.size() == max_pool_size) {
        int ret = pthread_cond_timedwait(&cond, &mutex, &ts);
        if (ret == ETIMEDOUT) {
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
    }
    if (ready_pool.empty()) {
        tcp_connection_t *connection = newConnection(flags);
        if (connection == NULL) {
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
        pool.push_back(connection);
        pthread_mutex_unlock(&mutex);
        return connection;
    } else {
        for (int count = 0; count < max_pool_size; ++count) {
            tcp_connection_t *connection = ready_pool.front();
            ready_pool.pop();
            int valid;
            pthread_rwlock_rdlock(&connection->rwlock);
            valid = connection->valid;
            pthread_rwlock_unlock(&connection->rwlock);
            if (valid == false) {
                ready_pool.push(connection);
                clock_gettime(CLOCK_MONOTONIC_COARSE, &end_time);
                int period = (end_time.tv_sec * 1000 + end_time.tv_nsec / 1000000) - (start_time.tv_sec * 1000 + start_time.tv_nsec / 1000000);
                if (period >= timeout) {
                    pthread_mutex_unlock(&mutex);
                    return NULL;
                }
            } else {
                pthread_mutex_unlock(&mutex);
                return connection;
            }
        }
        /* at last, all connection is failed */
        pthread_mutex_unlock(&mutex);
        return NULL;
    }
}

void CharlesTcpPool::putConnection(tcp_connection_t *connection) {
    pthread_mutex_lock(&mutex);
    ready_pool.push(connection);
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
}

int CharlesTcpPool::setConfig(int sock, int flags) {
    if (flags & CHARLES_SOCK_BLOCK) {
        if (-1 == set_nonblock(sock, false))
            return -1;
    }

    if (flags & CHARLES_SOCK_NONBLOCK) {
        if (-1 == set_nonblock(sock, true))
            return -1;
    }

    if (flags & CHARLES_TCP_DELAY) {
        if (-1 == set_tcp_nodelay(sock, false))
            return -1;
    }

    if (flags & CHARLES_TCP_NODELAY) {
        if (-1 == set_tcp_nodelay(sock, true))
            return -1;
    }

    if (flags & CHARLES_TCP_KEEPALIVE) {
        if (-1 == set_tcp_keepalive(sock, true))
            return -1;
    }

    if (flags & CHARLES_TCP_NOKEEPALIVE) {
        if (-1 == set_tcp_keepalive(sock, false))
            return -1;
    }

    return 0;
}

#include "charles_tcp_pool.h"

int main(int argc, char **argv) {
    CharlesTcpPool pool("127.0.0.1", 19920);
    pool.initPool();

    sleep(86400);

    return 0;
}

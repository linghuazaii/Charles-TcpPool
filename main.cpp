#include "charles_tcp_pool.h"
#include <signal.h>
using namespace std;

void sigpipe(int) {
    cout<<"receive sigpipe"<<endl;
}

int main(int argc, char **argv) {
    CharlesTcpPool pool("127.0.0.1", 19920);
    pool.initPool();
    const char *text = "hi there!";
    signal(SIGPIPE, sigpipe);
    while (true) {
        tcp_connection_t *connection = pool.getConnection(300);
        if (connection == NULL) {
            cout<<"hanging"<<endl;
            continue;
        }
        cout<<"is there a dead lock?"<<endl;
        int count = write(connection->fd, text, strlen(text));
        char buffer[256];
        if (count == -1) {
            cout<<"write error "<<strerror(errno)<<endl;
            pool.putConnection(connection);
            continue;
        }
        count = read(connection->fd, buffer, 256);
        if (count == -1) {
            cout<<"read error "<<strerror(errno)<<endl;
        } else if (count > 0)
            cout<<"count: "<<count<<"\tserver said: "<<buffer<<endl;
        pool.putConnection(connection);
        //sleep(1);
    }

    sleep(86400);

    return 0;
}

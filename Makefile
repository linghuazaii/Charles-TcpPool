src=threadpool.o main.o charles_tcp_pool.o
bin=test

cppflags=-g
cflags=-g
cc=g++
link=-lpthread
server=server

all:$(bin) $(server)

$(bin):$(src)
	$(cc) $^ -o $@ $(link)

$(server):server.cpp threadpool.c
	g++ $^ -o $@ -lpthread

%.o:%.cpp
	$(cc) $(cppflags) $^ -c -o $@

%.o:%.cpp
	$(cc) $(cflags) $^ -c -o $@

clean:
	-rm -f $(src) $(bin) $(server)

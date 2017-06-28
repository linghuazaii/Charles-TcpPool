src=threadpool.o charles_tcp_pool.o
test_src=main.o
test=test
library=libcharles_tcp_pool.so.1
lnso=libcharles_tcp_pool.so

cppflags=-fPIC -O3
cflags=-fPIC -O3
cc=g++
link=-lpthread
server=server

all:$(library) $(test) $(server)

$(library):$(src)
	$(cc) -shared -fPIC -Wl,-soname,$(library) $^ -o $@ $(link)
	-ln -s $(library) $(lnso)

$(test):$(test_src)
	$(cc) -L. $^ -o $@ $(link) -lcharles_tcp_pool

$(server):server.cpp threadpool.c
	g++ $^ -o $@ -lpthread

%.o:%.cpp
	$(cc) $(cppflags) $^ -c -o $@

%.o:%.c
	$(cc) $(cflags) $^ -c -o $@

clean:
	-rm -f $(src) $(test) $(server) $(test_src) $(library) $(lnso)

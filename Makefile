src=threadpool.o main.o charles_tcp_pool.o
bin=test

cppflags=-g
cflags=-g
cc=g++
link=-lpthread

all:$(bin)

$(bin):$(src)
	$(cc) $^ -o $@ $(link)

%.o:%.cpp
	$(cc) $(cppflags) $^ -c -o $@

%.o:%.cpp
	$(cc) $(cflags) $^ -c -o $@

clean:
	-rm -f $(src) $(bin)

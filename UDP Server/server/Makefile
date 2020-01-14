# This is a Makefile for PA1
# Chad Lewis

CXX = gcc

# runs for "make all"
all: server

# runs for "make server"
server: server.o
	$(CXX) udp_server.c -o server -w

server.o: udp_server.c
	$(CXX) -c udp_server.c -w


# runs for "make clean"
clean:
	rm server

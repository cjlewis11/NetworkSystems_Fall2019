# This is a Makefile for PA1
# Chad Lewis

CXX = gcc

# runs for "make all"
all: client

# runs for "make client"
client: client.o
	$(CXX) udp_client.c -o client -w

client.o: udp_client.c
	$(CXX) -c udp_client.c -w


# runs for "make clean"
clean:
	rm client

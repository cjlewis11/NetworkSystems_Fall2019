# This is a Makefile for PA1
# Chad Lewis

CXX = gcc

# runs for "make all"
all: webserver

# runs for "make server"
webserver: webserver.o
	$(CXX) -pthread -o webserver webserver.c

webserver.o: webserver.c
	$(CXX) -c webserver.c


# runs for "make clean"
clean:
	rm webserver

# This is a Makefile for PA1
# Chad Lewis

CXX = gcc

# runs for "make all"
all: dfs memcheck

# runs for "make webproxy"
dfs: dfs.o
	$(CXX) -pthread -o dfs dfs.c -lcrypto

dfs.o: dfs.c
	$(CXX) -c dfs.c

# runs for make memcheck
memcheck: dfs.o
	$(CXX) -g -pthread -o memcheckdfs dfs.c -lcrypto

memcheck.o: dfs.c
	$(CXX) -c dfs.c



# runs for "make clean"
clean:
	rm dfs memcheckdfs

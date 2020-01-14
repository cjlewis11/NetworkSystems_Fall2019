# This is a Makefile for PA1
# Chad Lewis

CXX = gcc

# runs for "make all"
all: dfc memcheck

# runs for "make dfc"
dfc: dfc.o
	$(CXX) dfc.c -o dfc -lssl -lcrypto

dfc.o: dfc.c
	$(CXX) -c dfc.c

# runs for make memcheck
memcheck: dfc.o
	$(CXX) -g -pthread -o memcheckdfc dfc.c -lcrypto

memcheck.o: dfc.c
	$(CXX) -c dfc.c

# runs for "make clean"
clean:
	rm dfc memchecdfc

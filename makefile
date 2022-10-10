CC=gcc
CFLAGS=-Wall -Wextra -Iincludes
LDLIBS=-lcrypto
VPATH=src

all: server client

server: server.o optparser.o hash.o

client: client.o optparser.o

clean:
	rm -f *~ *.o client server
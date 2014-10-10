CFLAGS=-g -Wall -std=c99 -lm
CC=gcc

all: cachesim

cachesim: cachesim.o cachesim_driver.o
	$(CC) $(CFLAGS) -o cachesim cachesim.o cachesim_driver.o

clean:
	rm -f cachesim *.o

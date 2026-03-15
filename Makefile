CC = gcc
CFLAGS = -Wall -Wextra -g

all: oss worker

oss: oss.o
	$(CC) oss.o -o oss

worker: worker.o
	$(CC) worker.o -o worker

oss.o: oss.c shared.h
	$(CC) -c oss.c $(CFLAGS) -o oss.o

worker.o: worker.c shared.h
	$(CC) -c worker.c $(CFLAGS) -o worker.o

clean:
	rm -f oss worker oss.o worker.o

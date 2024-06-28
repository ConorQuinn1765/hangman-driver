CC = gcc
CFLAGS = -Wall -Wextra -g
OBJS = test.o unitTest.o
EXE = test

$(EXE): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(EXE)

test.o: test.c
	$(CC) $(CFLAGS) -c test.c

unitTest.o: unitTest.c unitTest.h
	$(CC) $(CFLAGS) -c unitTest.c

clean:
	rm $(EXE) $(OBJS)

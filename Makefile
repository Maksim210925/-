CC=g++
CFLAGS=-Wall
LDFLAGS=-lev -lpthread

.PHONY: all
all: test3 test2 test1 clean

.PHONY: clean
clean:
	$(RM) *~ *.o

OBJECTS=test3.o
test3: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o test3 $(LDFLAGS)

OBJECTS1=test2.o
test2: $(OBJECTS1)
	$(CC) $(CFLAGS) $(OBJECTS1) -o test2 $(LDFLAGS)

OBJECTS2=test1.o
test1: $(OBJECTS2)
	$(CC) $(CFLAGS) $(OBJECTS2) -o test1 $(LDFLAGS)
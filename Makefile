CC     = gcc
CFLAGS = -g
TARGET1 = OSS
TARGET2 = Process
OBJS1   = oss.o stime.o queue.o
OBJS2   = process.o

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(OBJS1)
	$(CC) -o $(TARGET1) $(OBJS1)

oss.o: oss.c
	$(CC) $(CFLAGS) -c oss.c

stime.o: stime.c
	$(CC) $(CFLAGS) -c stime.c

queue.o: queue.c
	$(CC) $(CFLAGS) -c queue.c

$(TARGET2): $(OBJS2)
	$(CC) -o $(TARGET2) $(OBJS2)

process.o: process.c
	$(CC) $(CFLAGS) -c process.c

clean:
	/bin/rm -f *.o $(TARGET1) $(TARGET2) test.out

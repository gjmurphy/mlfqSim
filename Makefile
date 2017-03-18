CC     = gcc
CFLAGS = -g
TARGET1 = OSS
TARGET2 = Slave
OBJS1   = oss.o stime.o
OBJS2   = slave.o

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(OBJS1)
	$(CC) -o $(TARGET1) $(OBJS1)

oss.o: oss.c
	$(CC) $(CFLAGS) -c oss.c

stime.o: stime.c
	$(CC) $(CFLAGS) -c stime.c

$(TARGET2): $(OBJS2)
	$(CC) -o $(TARGET2) $(OBJS2)

slave.o: slave.c
	$(CC) $(CFLAGS) -c slave.c

clean:
	/bin/rm -f *.o $(TARGET1) $(TARGET2) test.out

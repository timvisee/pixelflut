CC=gcc
CFLAGS=-Wall -MMD
LDFLAGS=-levent

TARGET := pixelpipe
OBJS := util.o session.o main.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

-include *.d

clean:
	rm -f $(TARGET) *.o *.d

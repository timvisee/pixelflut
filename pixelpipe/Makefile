ACC=gcc
CFLAGS=-Wall
LDFLAGS=-levent

TARGET := pixelpipe
OBJS := main.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -f $(TARGET) $(OBJS)

CC=gcc
CFLAGS= -O0 -Wall -MMD --debug 
LDFLAGS=-levent -levent_pthreads -lGL -lGLEW -lSDL2 -pthread

TARGET := pixelpipe
OBJS := util.o session.o main.o net.o ui.o

$(TARGET): $(OBJS) Makefile
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

-include *.d

clean:
	rm -f $(TARGET) *.o *.d

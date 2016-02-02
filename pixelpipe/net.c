#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_struct.h>
#include <event2/buffer.h>
#include <event2/thread.h>
#include <event2/util.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdlib.h> /* for malloc */
#include <string.h> /* for memset */
#include <sys/time.h>

#include "session.h"

#define MAX_LINE 1024

static struct event_base *myevent_base;

static void net_cleanup();
static void on_accept(evutil_socket_t listener, short event, void *arg);

int net_init(int port) {
    atexit(net_cleanup);
    evthread_use_pthreads();
    event_enable_debug_mode();
    myevent_base = event_base_new();
    if (myevent_base == NULL) {
        perror("Libevent initialisazion failed");
        return 1;
    }

    struct event *listener_event;
    evutil_socket_t listener;

#ifdef USE_V6
    struct sockaddr_in6 addr;
    listener = socket(AF_INET6, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof (addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(port);
#else
    struct sockaddr_in addr;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof (addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
#endif

    if (listener < 0) {
        perror("Meh");
        return 1;
    }

    evutil_make_socket_nonblocking(listener);
    evutil_make_listen_socket_reuseable(listener);

    if (bind(listener, (struct sockaddr*) &addr, sizeof (addr)) < 0)
        return 1;

    if (listen(listener, 16) < 0)
        return 1;

    listener_event = event_new(myevent_base, listener,
            EV_READ | EV_PERSIST, on_accept, (void*) myevent_base);
    event_add(listener_event, NULL);
    return 0;
}

void net_loop(void *ptr) {
    event_base_dispatch(myevent_base);
}

void net_stop() {
    event_base_loopexit(myevent_base, NULL);
}

static void net_cleanup() {
    event_base_loopexit(myevent_base, NULL);
}

static void on_read(struct session *s) {
    struct evbuffer *input, *output;
    char *line;
    size_t n;
    input = bufferevent_get_input(s->buff_event);
    output = bufferevent_get_output(s->buff_event);

    while ((line = evbuffer_readln(input, &n, EVBUFFER_EOL_LF))) {
        unsigned int y,x,m,c,t1,t2;
        m = sscanf(line, "PX %u %u %n%8x%n", &x, &y, &t1, &c, &t2);
        if(m == 2) {
            printf("R %d %d",x,y);
        } else if(m == 3 && t2-t1 == 6) {
            c |= 0xff000000;
            printf("PX %u %u %x",x,y,c);
        } else if(m == 3 && t2-t1 == 8) {
            // #rrggbbaa -> #aarrggbb
            c = (c >> 8) + ((c & 0xff) << 24); 
            printf("PX %d %d %x",x,y,c);
        }
        evbuffer_add(output, line, n);
        evbuffer_add(output, "\n", 1);
        free(line);
    }

    if (evbuffer_get_length(input) >= MAX_LINE) {
        session_error(s, "Long line");
    }
}

static void on_write(struct session *s) {
    printf("writeeee\n");
}

static void on_error(struct session *s, short error) {
    if (error & BEV_EVENT_EOF) {
    } else if (error & BEV_EVENT_ERROR) {
    } else if (error & BEV_EVENT_TIMEOUT) {
        session_error(s, "Timeout");
    }
}

static void on_accept(evutil_socket_t listener, short event, void *arg) {
    struct event_base *base = arg;
    struct session *s;
    s = session_new(base);
    session_setcb(s, on_read, on_write, on_error);
    session_accept(s, listener);
}


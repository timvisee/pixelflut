#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <netinet/in.h>
#include <bits/socket.h>
#include <stdlib.h> /* for malloc */
#include <string.h> /* for memset */
#include <bits/time.h>
#include <signal.h>

#define USE_V6
#define PORT 1337
#define SINK_PORT 1338
#define MAX_LINE 1024

#define SESSION_ALIVE 2
#define SESSION_DYING 1
#define SESSION_DEAD  0

static const struct timeval READ_TIMEOUT = {.tv_sec = 60, .tv_usec = 0};

static void on_read(struct bufferevent *bev, void *ctx);
static void on_write(struct bufferevent *bev, void *ctx);
static void on_event(struct bufferevent *bev, short error, void *ctx);

struct session {
    evutil_socket_t sock_fd;
    int alive;
    struct sockaddr_storage *addr;
    struct bufferevent *buff_event;
    struct event_base *event_base;
    struct session *prev, *next;
};

struct sink {
    struct bufferevent *buff_event;
};

static struct session session_head = {.next = NULL};
static uint session_count = 0;

static int sockaddr_eq(struct sockaddr_storage *a, struct sockaddr_storage *b) {

#ifdef USE_V6
    uint8_t * x = ((struct sockaddr_in6 *) a)->sin6_addr.s6_addr;
    uint8_t * y = ((struct sockaddr_in6 *) b)->sin6_addr.s6_addr;
    return memcmp(x, y, 16) == 0;
#else
    in_addr_t * x = ((struct sockaddr_in *) a)->sin_addr.s_addr;
    in_addr_t * y = ((struct sockaddr_in *) b)->sin_addr.s_addr;
    return memcmp(x, y, sizeof (in_addr_t)) == 0;
#endif
}

void session_free(struct session * s);
void session_error(struct session * s, char* msg); 
void session_close(struct session * s); 

struct session* session_new(struct event_base * base, evutil_socket_t sockfd, struct sockaddr_storage *addr) {
    struct session * s; 

    // close old sessions (if any)
    for(s = session_head.next; s != NULL; s=s->next) {
        if (s->alive == SESSION_ALIVE && sockaddr_eq(addr, s->addr)) {
            session_error(s, "To many connections");
        }
    }

    s = calloc(1, sizeof (struct session));
    if (s == NULL)
        return NULL;

    s->alive = SESSION_ALIVE;
    s->prev = &session_head;
    s->next = session_head.next;
    if (session_head.next != NULL) {
        session_head.next->prev = s;
    }
    session_head.next = s;
    session_count++;

    s->sock_fd = sockfd;
    s->event_base = base;
    s->buff_event = bufferevent_socket_new(base, sockfd, BEV_OPT_CLOSE_ON_FREE);
    s->addr = addr;

    bufferevent_setcb(s->buff_event, on_read, on_write, on_event, s);
    bufferevent_setwatermark(s->buff_event, EV_READ, 0, MAX_LINE);
    bufferevent_set_timeouts(s->buff_event, &READ_TIMEOUT, NULL);
    bufferevent_enable(s->buff_event, EV_READ | EV_WRITE);

    printf("new: %d\n", session_count);

    return s;
};

void session_error(struct session * s, char* msg) {
    struct evbuffer *tmp = bufferevent_get_output(s->buff_event);
    evbuffer_add_printf(tmp, "ERR %s\n", msg);
    session_close(s);
}

void dead_write(struct bufferevent *bev, void *ctx) {
    if(evbuffer_get_length(bufferevent_get_output(bev)) == 0) {
        struct session *s = (struct session *) ctx;
        s->alive = SESSION_DEAD; 
        session_free(s);
    }
}

void dead_event(struct bufferevent *bev, short error, void *ctx) {
    session_free((struct session*) ctx);
}

void session_close(struct session *s) {
    if(s->alive == SESSION_ALIVE) {
        s->alive = SESSION_DYING;
        bufferevent_disable(s->buff_event, EV_READ);
        bufferevent_flush(s->buff_event, EV_WRITE, BEV_FINISHED);
        bufferevent_setcb(s->buff_event, NULL, dead_write, dead_event, s);
    }
}

void session_free(struct session * s) {
    if (s == NULL) return;
    session_close(s);

    if (s->next != NULL)
        s->next->prev = s->prev;
    s->prev->next = s->next;

    if (s->buff_event != NULL)
        bufferevent_free(s->buff_event);

    if (s->addr != NULL)
        free(s->addr);

    free(s);
    session_count--;
    printf("free: %d\n", session_count);
};

void on_accept(evutil_socket_t listener, short event, void *arg) {
    struct event_base *base = arg;
    struct sockaddr_storage *addr;
    int fd;
    socklen_t slen = sizeof (addr);

    addr = calloc(1, slen);

    if ((fd = accept(listener, (struct sockaddr*) addr, &slen)) < 0) {
        free(addr);
        perror("on_accept failed.");
        return;
    }

    evutil_make_socket_nonblocking(fd);
    session_new(base, fd, addr);
}

void on_read(struct bufferevent *bev, void *ctx) {
    struct session *s = (struct session *) ctx;
    struct evbuffer *input, *output;
    char *line;
    size_t n;
    input = bufferevent_get_input(bev);
    output = bufferevent_get_output(bev);

    while ((line = evbuffer_readln(input, &n, EVBUFFER_EOL_LF))) {
        evbuffer_add(output, line, n);
        evbuffer_add(output, "\n", 1);
        free(line);
    }

    if (evbuffer_get_length(input) >= MAX_LINE) {
        session_error(s, "Long line");
    }
}

void on_write(struct bufferevent *bev, void *ctx) {
    printf("writeeee\n");
}

void on_event(struct bufferevent *bev, short error, void *ctx) {
    if (error & BEV_EVENT_EOF) {
    } else if (error & BEV_EVENT_ERROR) {
    } else if (error & BEV_EVENT_TIMEOUT) {
    }
    session_close((struct session *) ctx);
}

struct event* setup_listener(struct event_base *base) {
    struct event *listener_event;
    evutil_socket_t listener;

#ifdef USE_V6
    struct sockaddr_in6 addr;
    listener = socket(AF_INET6, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof (addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(PORT);
#else
    struct sockaddr_in addr;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof (addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
#endif

    if (listener < 0) {
        perror("Meh");
        return NULL;
    }

    evutil_make_socket_nonblocking(listener);
    evutil_make_listen_socket_reuseable(listener);

    if (bind(listener, (struct sockaddr*) &addr, sizeof (addr)) < 0)
        return NULL;

    if (listen(listener, 16) < 0)
        return NULL;

    listener_event = event_new(base, listener,
            EV_READ | EV_PERSIST, on_accept, (void*) base);
    event_add(listener_event, NULL);
    return listener_event;
}

static void
close_on_finished_writecb(struct bufferevent *bev, void *ctx) {
    struct evbuffer *b = bufferevent_get_output(bev);
    if (evbuffer_get_length(b) == 0) {
        bufferevent_free(bev);
    }
}

void on_shutdown(evutil_socket_t fd, short what, void *arg) {
    struct session *cur;
    for (cur = session_head.next; cur != NULL; cur = cur->next) {
        bufferevent_disable(cur->buff_event, EV_READ);

        struct evbuffer *tmp = bufferevent_get_output(cur->buff_event);
        evbuffer_add_printf(tmp, "ERR DISCONNECT\n");
        bufferevent_flush(cur->buff_event, EV_WRITE, BEV_FINISHED);
        bufferevent_setcb(cur->buff_event,
                NULL, close_on_finished_writecb, on_event, NULL);

    }
    event_base_loopexit((struct event_base *) arg, NULL);
}

int main(int args, char* argv[]) {
    struct event_base *base;
    setvbuf(stdout, NULL, _IONBF, 0);

    event_enable_debug_mode();
    base = event_base_new();
    if (base == NULL) {
        perror("Libevent initialisazion failed");
        return 1;
    }

    struct event *sig_event;
    sig_event = evsignal_new(base, SIGINT, on_shutdown, (void*) base);
    event_add(sig_event, NULL);

    struct event *listener;
    if ((listener = setup_listener(base)) == NULL) {
        perror("Network setup");
        return 1;
    }

    event_base_dispatch(base);

    return 0;
}

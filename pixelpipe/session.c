#include "session.h"

static const struct timeval DEAD_TIMEOUT = {.tv_sec = 10, .tv_usec = 0};
static const struct timeval READ_TIMEOUT = {.tv_sec = 60, .tv_usec = 0};
struct session session_head = {.next=NULL};
unsigned int session_count = 0; 

static void on_read(struct bufferevent *bev, void *ctx) {
    struct session *s = (struct session *) ctx;
    if(s->readcb != NULL) {
        (s->readcb)(s);
    }
}

static void on_write(struct bufferevent *bev, void *ctx) {
    struct session *s = (struct session *) ctx;
    if(s->writecb != NULL) {
        (s->writecb)(s);
    }
}

static void on_event(struct bufferevent *bev, short error, void *ctx) {
    struct session *s = (struct session *) ctx;
    if(s->errorcb != NULL) {
        (s->errorcb)(s, error);
    }
    session_close(s);
}

struct session* session_new(struct event_base * base) {
    struct session * s; 
    s = calloc(1, sizeof (struct session));
    if (s == NULL)
        return NULL;

    s->mode = SESSION_NEW;
    s->event_base = base;
    s->prev = &session_head;
    s->next = session_head.next;
    if (session_head.next != NULL) {
        session_head.next->prev = s;
    }
    session_head.next = s;
    session_count++;
    return s;
}

void session_accept(struct session *s, evutil_socket_t sockfd) {
    struct sockaddr_storage *addr;
    socklen_t slen = sizeof (addr);
    int fd;

    addr = calloc(1, slen);
    if ((fd = accept(sockfd, (struct sockaddr*) addr, &slen)) < 0) {
        free(addr);
        return;
    }

    session_connect(s, fd, addr);
}

void session_setcb(struct session *s,
                   void(*readcb)(struct session *),
                   void(*writecb)(struct session *),
                   void(*errorcb)(struct session *, short)) {
  s->readcb = readcb;
  s->writecb = writecb;
  s->errorcb = errorcb;
}

void session_connect(struct session *s,
                     evutil_socket_t sockfd,
                     struct sockaddr_storage *addr) {
    evutil_make_socket_nonblocking(sockfd);
    s->mode = SESSION_ALIVE;
    s->buff_event = bufferevent_socket_new(s->event_base, sockfd, BEV_OPT_CLOSE_ON_FREE);
    s->addr = addr;

    bufferevent_setcb(s->buff_event, on_read, on_write, on_event, s);
    bufferevent_setwatermark(s->buff_event, EV_READ, 0, MAX_READBUFF);
    bufferevent_set_timeouts(s->buff_event, &READ_TIMEOUT, NULL);
    bufferevent_enable(s->buff_event, EV_READ | EV_WRITE);
};

void session_error(struct session * s, char* msg) {
    struct evbuffer *tmp = bufferevent_get_output(s->buff_event);
    evbuffer_add_printf(tmp, "ERR %s\n", msg);
    session_close(s);
}

static void dead_write(struct bufferevent *bev, void *ctx) {
    if(evbuffer_get_length(bufferevent_get_output(bev)) == 0) {
        struct session *s = (struct session *) ctx;
        s->mode = SESSION_DEAD; 
        session_free(s);
    }
}

static void dead_event(struct bufferevent *bev, short error, void *ctx) {
    session_free((struct session*) ctx);
}

void session_close(struct session *s) {
    if(s->mode == SESSION_NEW) {
        s->mode = SESSION_DEAD;
        session_free(s);
    } else if(s->mode == SESSION_ALIVE) {
        s->mode = SESSION_DYING;
        bufferevent_disable(s->buff_event, EV_READ);
        bufferevent_flush(s->buff_event, EV_WRITE, BEV_FINISHED);
        bufferevent_setcb(s->buff_event, NULL, dead_write, dead_event, s);
        bufferevent_set_timeouts(s->buff_event, &DEAD_TIMEOUT, &DEAD_TIMEOUT);
        dead_write(s->buff_event, s);
    }
}

void session_free(struct session * s) {
    if (s == NULL) return;
    session_close(s);

    if (s->next != NULL)
        s->next->prev = s->prev;
    s->prev->next = s->next;

    if (s->freecb)
      s->freecb(s);
    else if(s->user != NULL)
      free(s->user);

    if (s->buff_event != NULL)
        bufferevent_free(s->buff_event);

    if (s->addr != NULL)
        free(s->addr);

    free(s);
    session_count--;
};

/* Takes ownership over the char array. Refcount starts with 1. You MUST call
    session_decref() on the returned struct at some time, otherwise it will never
    be deleted. 
*/
struct refcounted* session_make_refcount(char *data, int len) {
    struct refcounted* ref = calloc(1, sizeof(struct refcounted));
    ref->refs = 1;
    ref->size = len;
    ref->data = data;
    return ref;
};

static void _session_cleanup_refc(const void *data, size_t datalen, void *extra) {
    struct refcounted *ref = (struct refcounted*) extra;
    session_decref(ref);
}

void session_decref(struct refcounted *ref) {
    ref->refs--;
    if(ref->refs==0) {
        free(ref->data);
        free(ref);
    }
}

void session_send_ref(struct session * s, struct refcounted * ref) {
    if(s->mode == SESSION_ALIVE) {
        struct evbuffer *output = bufferevent_get_output(s->buff_event);
        if(evbuffer_add_reference(output, ref->data, ref->size,
                              _session_cleanup_refc, ref) == 0) {
            ref->refs++;
        }
    } 
};


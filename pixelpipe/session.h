#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdlib.h> /* for malloc */
#include <string.h> /* for memset */
#include <sys/time.h>

#define SESSION_NEW   3
#define SESSION_ALIVE 2
#define SESSION_DYING 1
#define SESSION_DEAD  0
#define MAX_READBUFF 1024

struct session {
    int mode;
    void(*readcb)(struct session *);
    void(*writecb)(struct session *);
    void(*errorcb)(struct session *, short);
    struct sockaddr_storage *addr;
    struct bufferevent *buff_event;
    struct event_base *event_base;
    struct session *prev, *next;
};

struct sink {
    struct bufferevent *buff_event;
};

struct session session_head;
unsigned int session_count;

struct session* session_new(struct event_base * base);
void session_accept(struct session *s, evutil_socket_t listen_sock);
void session_connect(struct session *s, evutil_socket_t sockfd, struct sockaddr_storage *addr);
void session_setcb(struct session *s,
                   void(*readcb)(struct session *),
                   void(*writecb)(struct session *),
                   void(*errorcb)(struct session *, short));
void session_error(struct session * s, char* msg);
void session_close(struct session *s);
void session_free(struct session * s);

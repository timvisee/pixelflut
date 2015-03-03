
#include "util.h"

static int sockaddr_eq(struct sockaddr_storage *a, struct sockaddr_storage *b) {
    if(a->ss_family == AF_INET6 && b->ss_family == AF_INET6) {
        uint8_t * x = ((struct sockaddr_in6 *) a)->sin6_addr.s6_addr;
        uint8_t * y = ((struct sockaddr_in6 *) b)->sin6_addr.s6_addr;
        return memcmp(x, y, 16) == 0;
    } else if (a->ss_family == AF_INET && b->ss_family == AF_INET) {
        in_addr_t * x = &((struct sockaddr_in *) a)->sin_addr.s_addr;
        in_addr_t * y = &((struct sockaddr_in *) b)->sin_addr.s_addr;
        return memcmp(x, y, sizeof (in_addr_t)) == 0;
    } else {
        return -1;
    }
}

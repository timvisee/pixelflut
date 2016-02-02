#include <pthread.h>
#include <stdio.h>
#include "ui.h"
#include "net.h"
#include "session.h"

#define USE_V6
#define PORT 1337
#define MAX_LINE 1024

int main(int args, char* argv[]) {
    pthread_t netThread;
    setvbuf(stdout, NULL, _IONBF, 0);

    if(ui_init() || net_init(PORT)) {
        return 1;
    }

    if(pthread_create(&netThread, NULL, (void *) &net_loop, NULL)) {
        return 1;
    }

    pthread_detach(netThread);
 
    ui_loop();
    net_stop();
    pthread_exit(NULL);
    return 0;
}   


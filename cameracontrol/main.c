#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <math.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/signal.h>

#include "cJSON.h"
#include "uvc.h"
#include "log.h"
#include "websocket.h"

static void sighandler(int sig) {
    if ((sig == SIGINT) || (sig == SIGTERM)) {
	websocket_shutdown();
    }
}

int main(int argc, char *argv[]) {
    int websocket_port = 40404;
    pid_t pid = 0;
    if (argc == 2) {
	websocket_port = atoi(argv[1]);
    }

    if (strstr(argv[0], "cameracontrol-debug")) {
	LOG_USE_SYSLOG(0);
	LOG_SET_LOGLEVEL(3);
    } else {
	if (strstr(argv[0], "cameracontrol-silent")) {
	    LOG_USE_SYSLOG(1);
	    LOG_SET_LOGLEVEL(0);
	} else {
	    LOG_USE_SYSLOG(1);
	    LOG_SET_LOGLEVEL(3);
	}
	pid = fork();
	if (pid == 0) {
    	    // child
    	    freopen("/dev/null", "r", stdin);
    	    freopen("/dev/null", "w", stdout);
	    freopen("/dev/null", "w", stderr);
    	    pid = setsid();
	} else if (pid < 0) {
    	    /* failed to fork */
	    return -1;
	} else {
	    /* parent */
	    _exit(0);
	}
    }

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    uvc_init();
    websocket_init(websocket_port);

    for (;;) {
	websocket_service(20);
	if (!websocket_is_running()) {
	    break;
	}
	
    }

    if (websocket_is_running()) {
	websocket_shutdown();
    }
    uvc_shutdown();

    /* make valgrind happy */
    close(0);
    close(1);
    close(2);
    return 0;
}

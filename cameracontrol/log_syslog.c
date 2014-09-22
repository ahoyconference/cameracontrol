#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>

void log_syslog_error(char *msg) {
    syslog(LOG_ERR, "%s", msg);
}

void log_syslog_verbose(char *msg) {
    syslog(LOG_INFO, "%s", msg);
}

void log_syslog_debug(char *msg) {
    syslog(LOG_DEBUG, "%s", msg);
}


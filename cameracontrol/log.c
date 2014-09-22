#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "log_syslog.h"

#define USE_SYSLOG 1
static int log_level = 0;
static int log_use_syslog = 0;

void LOG_ERROR(char *fmt, ...) {
    char tmp[32000];
    va_list ap;
    char log_msg[32000];
    time_t t;
    struct tm tm;
    char date_format[50] = "%G-%m-%d %T";
    char date[100];

    if (log_level > 0) {
	va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	if (log_use_syslog) {
	    log_syslog_error(tmp);
	} else {
	    time(&t);
	    localtime_r(&t, &tm);
	    strftime(date, sizeof(date), date_format, &tm);
	    snprintf(log_msg, sizeof(log_msg) - 1, "%s|%s", date, tmp);
	    fputs(log_msg, stdout);
	    fflush(stdout);
	}
    }
}

void LOG_VERBOSE(char *fmt, ...) {
    char tmp[32000];
    va_list ap;
    char log_msg[32000];
    time_t t;
    struct tm tm;
    char date_format[50] = "%G-%m-%d %T";
    char date[100];
    
    if (log_level >= 1) {
	va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	if (log_use_syslog) {
	    log_syslog_verbose(tmp);
	} else {
	    time(&t);
	    localtime_r(&t, &tm);
	    strftime(date, sizeof(date), date_format, &tm);
	    snprintf(log_msg, sizeof(log_msg) - 1, "%s|%s", date, tmp);
    	    fputs(log_msg, stdout);
	    fflush(stdout);
	}
    }
}

void LOG_DEBUG(char *fmt, ...) {
    char tmp[32000];
    va_list ap;
    char log_msg[32000];
    time_t t;
    struct tm tm;
    char date_format[50] = "%G-%m-%d %T";
    char date[100];
    
    if (log_level >= 2) {
	va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
        va_end(ap);
	if (log_use_syslog) {
	    log_syslog_debug(tmp);
	} else {
	    time(&t);
	    localtime_r(&t, &tm);
	    strftime(date, sizeof(date), date_format, &tm);
	    snprintf(log_msg, sizeof(log_msg) - 1, "%s|%s", date, tmp);
	    fputs(log_msg, stdout);
	    fflush(stdout);
	}
    }
}

void LOG_DEVDEBUG(char *fmt, ...) {
    char tmp[32000];
    va_list ap;
    char log_msg[32000];
    time_t t;
    struct tm tm;
    char date_format[50] = "%G-%m-%d %T";
    char date[100];

    if (log_level >= 5) {
	va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
        va_end(ap);
	snprintf(log_msg, sizeof(log_msg) - 1, "%s|%s", date, tmp);
	if (log_use_syslog) {
	    log_syslog_debug(tmp);
	} else {
	    time(&t);
	    localtime_r(&t, &tm);
	    strftime(date, sizeof(date), date_format, &tm);
	    snprintf(log_msg, sizeof(log_msg) - 1, "%s|%s", date, tmp);
	    fputs(log_msg, stdout);
	    fflush(stdout);
	}
    }
}

void LOG_SET_LOGLEVEL(int level) {
    log_level = level;
}

void LOG_USE_SYSLOG(int use) {
    log_use_syslog = use;
}

#ifndef __logger_h_
#define __logger_h_

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <limits.h>  /* LONG_MIN et al */
#include <stdio.h>   /* for printf */
#include <stdlib.h>  /* for exit */
#include <string.h>  /* memset */
#include <sys/socket.h> // socket
#include <sys/types.h>  // socket
#include <stdarg.h> //para el parametro ...

typedef enum { DEBUG=0, INFO, LOG_ERROR, FATAL } LOG_LEVEL;

extern LOG_LEVEL current_level;
extern bool error_flag;

/**
*  Minimo nivel de log a registrar. Cualquier llamada a log con un nivel mayor a newLevel sera ignorada
**/
void
setLogLevel(LOG_LEVEL newLevel);

void log_msg(LOG_LEVEL level, const char *fmt, ...);

#endif

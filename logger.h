#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
// Defines
#ifndef LOG_LEVEL
#define LOG_LEVEL NOTIFY
#endif//LOG_LEVEL
// Errors
#define ERR_CANNOT_OPEN_DISPLAY 1
// Log levels
#define DEBUG	4
#define INFO	3
#define NOTIFY	2
#define WARN	1
#define ERROR	0

#define debug(args...)	logger(DEBUG, args)
#define error(args...)	logger(ERROR, args)
#define info(args...)	logger(INFO, args)
#define warn(args...)	logger(WARN, args)
#define notify(args...)	logger(NOTIFY, args)

#define die(status,args...) {error(args); exit(status);}

void logger(int level, const char* format, ...) {
	va_list args;
	if (level > LOG_LEVEL)
		return;
	switch(level) {
		case DEBUG: fprintf(stderr, "\033[0;34m[DEBUG]"); break;
		case ERROR: fprintf(stderr, "\033[0;31m[ERROR]"); break;
		case INFO: fprintf(stderr, "\033[0;32m[INFO]"); break;
		case WARN: fprintf(stderr, "\033[0;33m[WARN]"); break;
		case NOTIFY: fprintf(stderr, "\033[0;35m[NOTIFY]"); break;
		default: fprintf(stderr, "\033[0;35m[LOG]"); break;
	}
	va_start(args, format);
	fprintf(stderr, " \033[0;20m");
	vfprintf(stderr, format, args);
	va_end(args);
}

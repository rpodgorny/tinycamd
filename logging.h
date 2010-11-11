#ifndef LOGGING_IS_IN
#define LOGGING_IS_IN

void log_f( const char *format, ...);
void fatal_f( const char *format, ...) __attribute__((noreturn));

#endif


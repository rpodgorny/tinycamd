#include "tinycamd.h"
#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static int syslog_opened = 0;

static void out_f( int priority, const char *format, va_list args)
{
  if ( !daemon_mode) {
    vfprintf(stderr, format, args);
  } else {
    if ( !syslog_opened) {
      openlog("tinycamd", 0, LOG_DAEMON);
      syslog_opened = 1;
    }
    vsyslog( priority, format, args);
  }
}

void log_f( const char *format, ...)
{
  va_list args;

  if ( !verbose) return;
  va_start( args, format);
  out_f( LOG_NOTICE, format, args);
  va_end(args);
}

void fatal_f( const char *format, ...)
{
  va_list args;

  va_start( args, format);
  out_f( LOG_ERR, format, args);
  va_end(args);

  exit(1);
}

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* strnlen() is a GNU extension; on old Sun Studio compilers (e.g., 5.12),  */
/* it does not exist.  Oracle did add it later, but when?  Until someone    */
/* can add a test for the correct version of the Sun Studio compiler that   */
/* added strnlen(), we use 0x5130 as the threshold, which should eliminate  */
/* any patch level of the 5.12 compiler.  This code is from mole strnlen.c. */

#ifdef __SUNPRO_C
#if __SUNPRO_C < 0x5130
size_t strnlen(register const char *s, register size_t maxlen)
{
  const char *end= (const char *)memchr(s, '\0', maxlen);
  return end ? (size_t) (end - s) : maxlen;
}
#endif /* __SUNPRO_C < 0x5130 */
#endif /* __SUNPRO_C */

#include "earthworm.h"

/* default log flags */
const char loggingFlag[4] = "tpo";

#define LOGBUFFER_LEN 512
static char logBuffer[LOGBUFFER_LEN];

void logMessage(char *fmt, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 1, 2)))
#endif
;

void logInitialize(char *configFile, int logFlag) {
  logit_init(configFile, 0, 1024, logFlag);
  logBuffer[0] = '\0';
}


void logMessage(char *fmt, ...) {
  size_t n;
  auto va_list l;
  va_start(l, fmt);

  /* if something is in the buffer, combine what we just got with the buffer */
  n = strnlen( logBuffer, LOGBUFFER_LEN );
  vsnprintf( &logBuffer[n], LOGBUFFER_LEN - n, fmt, l );

  va_end(l);

  /* If we have a newline at the end, we'll send it to the logging system */
  /* and clear the buffer */ 
  if(logBuffer[strlen(logBuffer)-1] == '\n') {
    logit( loggingFlag, "%s", logBuffer );
    logBuffer[0] = '\0';
  } 
}

/*
 * debug.c
 * Ryan Mallon (2005)
 *
 * Debug routines for compiler
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "cflags.h"

extern compiler_options_t cflags;

/*
 * Debug print
 */
void debug_printf(int level, char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  if(cflags.debug_level >= level)
    vprintf(fmt, args);

  va_end(args);
}

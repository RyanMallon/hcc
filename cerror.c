/*
 * cerror.c
 * Ryan Mallon (2005)
 *
 * Compiler Error Module
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "cerror.h"

extern char *infile;

#ifdef _DEBUG_ERRORS
#undef compiler_error
#endif

void compiler_error(int type, int line_num, char *fmt, ...) {
  va_list args;

  va_start(args, fmt);

  switch(line_num) {
  case CERROR_UNKNOWN_LINE:
    fprintf(stderr, "%s:Unknown line: ", infile);
    break;
  case CERROR_NO_LINE:
    fprintf(stderr, "%s: ", infile);
    break;
  default:
    fprintf(stderr, "%s:%d: ", infile, line_num + 1);
    break;
  }

  fprintf(stderr, "%s: ", (type == CERROR_ERROR) ? "error" : "warning");

  vfprintf(stderr, fmt, args);
  va_end(args);

  if(type == CERROR_ERROR)
    exit(EXIT_FAILURE);
}

/*
 * cerror.h
 * Ryan Mallon (2005)
 *
 */
#ifndef _CERROR_H_
#define _CERROR_H_

typedef enum {
  WARN_VAR_REDEFINE,
  WARN_FUNC_REDEFINE,
  WARN_ARG_REDEFINE,
  ERROR_VAR_UNDEFINED,
  ERROR_FUNC_UNDEFINED,
  ERROR_FUNC_TOO_FEW_ARGS,
  ERROR_FUNC_TOO_MANY_ARGS,
  ERROR_FUNC_ARG_TYPE,

  CERROR_NUM_MAX
} warning_t;

enum {
  CERROR_ERROR,
  CERROR_WARN
};

#define CERROR_UNKNOWN_LINE -1
#define CERROR_NO_LINE -2

static const char *warning_string[] = {
  "variable %s redefined",
  "function %s redefined",
  "duplicate argument name (%s)",
  "variable %s undefined",
  "function %s undefined",
  "function %s has too few arguments",
  "function %s has too many arguments",
  "function %s conflicts with declaration"
};

void compiler_error(int, int, char *, ...);

#ifdef _DEBUG_ERRORS
#include <stdio.h>
#define __cerror__ compiler_error
#define compiler_error(x, y, z, ...) do { \
    fprintf(stderr, "[%s:%d] ", __FILE__, __LINE__);	\
    __cerror__(x, y, z, ##__VA_ARGS__);	  \
  } while(0)
#endif /* _DEBUG_ERRORS */

#endif /* _CERROR_H_ */

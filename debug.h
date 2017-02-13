/*
 * debug.h
 * Ryan Mallon (2005)
 *
 * Debugging stuff
 *
 */
#ifndef _DEBUG_H_
#define _DEBUG_H_

void debug_printf(int, char *, ...);

#ifdef _DEBUG_DEBUG
#include <stdio.h>
#define __debug_printf__ dprintf
#define debug_printf(x, y, ...) do {			\
    fprintf(stderr, "[%s:%d]\n", __FILE__, __LINE__);	\
    __dprintf__(x, y, ##__VA_ARGS__);                   \
  } while(0)
#endif

#endif /* _DEBUG_H_ */

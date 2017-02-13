/*
 * rtlib/history.c
 * Ryan Mallon (2006)
 *
 * Runtime history functions. Can be compiled with either HCC or a standard
 * C compiler. Note that HCC doesn't support the shortcut operators (++, +=,
 * etc) or single line blocks with no braces.
 *
 */
#ifdef __HCC__
# define __STATIC
# define __VAR var
#else
# include "history.h"
# define __VAR
# define __STATIC static
#endif

/*
 * Primitive (scalar) history storage. Returns the new pointer value.
 * The new current variable is assigned by HCC.
 */
void __hist_p_store__(int *addr, int **ptr, int current, int depth) {

  /* Overwrite oldest history and update pointer */
  *ptr = *ptr - 1;
  if(*ptr < addr) {
    *ptr = addr + depth - 1;
  }

  **ptr = current;
}

/*
 * Primitive (scalar) history load.
 */
int __hist_p_load__(int *addr, int current, int *ptr, int depth, int history) {
  if(history == 0) {
    return current;
  }

  /* Find the position in the cycle */
  ptr = ptr + (history - 1);

  if(ptr >= addr + depth) {
    ptr = ptr - depth;
  }

  return *ptr;
}

/*
 * Primitive flat history storage.
 */
void __hist_fp_store__(int *addr, int depth, int value) {
  __VAR int *base;

  base = addr;
  addr = addr + depth + 1;

  addr = addr - 1;
  while(addr > base) {
    *addr = *(addr - 1);
    addr = addr - 1;
  }

  *base = value;
}

/*
 * Primitive flat history load
 */
int __hist_fp_load__(int *addr, int depth) {
  return *(addr + depth);
}

/*
 * Return the position of the oldest history in the cycle
 */
__STATIC void oldest_history(int *addr, int **ptr, int depth) {
  *ptr = *ptr - 1;

  if(*ptr < addr) {
    *ptr = addr + depth;
  }
}

/*
 * rtlib/harray.c
 * Ryan Mallon (2006)
 *
 * Runtime history functions for arrays.
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
 * Initialise an index-wise array. Takes O(n) time
 */
void __hist_iw_init__(int *addr, int **ptr, int size) {
  __VAR int i;

  for(i = 0; i < size; i = i + 1) {
    *(ptr + i) = addr + i;
  }
}

/*
 * Store to a fast index-wise array.
 */
void __hist_iw_store__(int *addr, int **ptr, int *current, int index,
		       int size, int depth, int value) {
  __VAR int *base, **idx, *cidx;

  /* Base of the current index */
  base = addr + (size * index);

  /* Common subexpression elimination. HCC doesnt do this */
  idx = ptr + index;
  cidx = current + index;

  /* Assign the new value and update the cycle */
  *idx = *idx - 1;
  if(*idx < base) {
    *idx = base + (depth - 1);
  }

  /* Assign the value */
  **idx = *cidx;
  *cidx = value;
}

/*
 * Load a value from a fast index-wise array. Returns the base address.
 */
int __hist_iw_load__(int *addr, int *ptr, int *current, int index,
		     int depth, int history, int size) {
  __VAR int *pos, *base;

  /* Get the copy value if the history is 0 */
  if(history == 0) {
    return index;
  }

  base = addr + (size * index);
  pos = ptr + index;

  pos = pos + (history - 1);
  if(pos >= base + depth) {
    pos = pos - depth;
  }

  return pos;
}

/*
 * Initialise an array-wise array for use with the O(d) algorithm
 */
void __hist_daw_init__(int *addr, int **ptr, int *clist, int **clist_ptr,
		       int depth) {
  __VAR int i;

  *ptr = addr;
  *clist_ptr = clist;

  /* Initialise the change-list */
  for(i = 0; i < (depth * 2); i = i + 2) {
    *(clist + i) = 999;
  }
}

/*
 * Store a value to an array-wise array using the O(n) algorithm
 */
void __hist_naw_store__(int *addr, int **ptr, int *current,
			int depth, int size, int index, int value) {
  __VAR int i, *oldest;

  /* Find the oldest history */
  *ptr = *ptr - size;
  if(*ptr < addr) {
    *ptr = addr + ((depth - 1) * size);
  }

  /* Copy the current array to the oldest */
  for(i = 0; i < size; i = i + 1) {
    *(*ptr + i) = *(current + i);
  }

  /* Assign the new value */
  current[index] = value;
}

/*
 * Store a value to an array-wise array using the O(d) algorithm
 */
void __hist_daw_store__(int *addr, int **ptr, int *current, int depth,
			int size, int *clist, int **clist_ptr, int index,
			int value) {
  __VAR int i;

  /* Find the oldest history */
  *ptr = *ptr - size;
  if(*ptr < addr) {
    *ptr = addr + ((depth - 1) * size);
  }

  /* Apply the change list */
  for(i = 0; i < depth; i = i + 1) {
    /* Apply change item */
    if(**clist_ptr != 999) {
      *(*ptr + **clist_ptr) = *(*clist_ptr + 1);
    }

    /* Move change list pointer */
    *clist_ptr = *clist_ptr + 2;
    if(*clist_ptr >= clist + (depth * 2)) {
      *clist_ptr = clist;
    }
  }

  /* Write the assignment to the change list */
  *clist_ptr = *clist_ptr - 2;
  if(*clist_ptr < clist) {
    *clist_ptr = clist + ((depth * 2) - 2);
  }

  **clist_ptr = index;
  *(*clist_ptr + 1) = value;

  /* Assign the new value */
  *(current + index) = value;
}

/*
 * Load a value from an array-wise array. Returns the base
 * address of the array.
 */
int *__hist_aw_load__(int *addr, int *current, int *ptr, int depth,
		      int size, int index, int history) {

   if(history == 0) {
     return current;
   }

   ptr = ptr + ((history - 1) * size);

   if(ptr >= addr + (size * depth)) {
      ptr = ptr - (size * depth);
   }

   return ptr;
}

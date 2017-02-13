/*
 * Ptr.hc
 *
 * Test for pointers on lhs
 *
 */

func int *__hist_aw_load__(int *addr, int *current, int *ptr, int depth,
			   int size, int index, int history);

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

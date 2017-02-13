func inline void oldest_history(int *addr, int **ptr, int depth);
func void foo();

/*
 * Return the position of the oldest history in the cycle
 */
void oldest_history(int *addr, int **ptr, int depth) {
  *ptr = *ptr - 1;

  if(*ptr < addr) {
    *ptr = addr + depth;
  }
}

void foo() {
  var int *addr, *ptr, depth;


  oldest_history(addr, &ptr, depth);
}

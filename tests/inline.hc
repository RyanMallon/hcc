/*
 * tests/inline.hc
 * Ryan Mallon (2006)
 *
 * Test function inlining
 *
 */
#include <print.h>

func inline void foo(int **a);

void foo(int **a) {
  printf("a = %p, *a = %p, **a = %d\n", a, *a, **a);

  *a = *a + 1;

}

void main() {
  var int a[3], x, *p, **dp;

  a[0] = 1;
  a[1] = 2;
  a[2] = 3;

  /*
  p = &a;
  dp = &p;

  foo(dp);
  foo(&p);

  printf("--\ndp = %p, p = %p, x = %d\n", dp, p, x);
  */
}


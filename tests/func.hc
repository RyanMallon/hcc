/*
 * tests/func.hc
 * Ryan Mallon (2006)
 *
 * Test passing history variables to functions
 *
 */
#include <print.h>

func int foo(int a);

int foo(int a) {
  printf("a = %d\n", a);
}

int main() {
  var int x<:2:>;

  x = 1;
  x = 2;
  x = 3;

  foo(x);
  foo(x<:1:>);
  foo(x<:2:>);
}

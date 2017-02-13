/*
 * tests/atomic.hc
 * Ryan Mallon (2006)
 *
 * Test atomic blocks
 *
 */
#include <print.h>

int main() {
  var int a<:2:>;

  a = 1;
  a = 2;
  a = 3;

  printf("a = <%d, %d, %d>\n", a, a<:1:>, a<:2:>);

  atomic {
    a = 4;
    a = 5;
    a = 6;
  }

  printf("a = <%d, %d, %d>\n", a, a<:1:>, a<:2:>);

}

/*
 * tests/hptr.hc
 * Ryan Mallon (2006)
 *
 * Test indirect accessing of history variables
 *
 */
#include <print.h>

void main() {
  var int h<:2:>, *p;

  p = &h;

  h = 1;
  h = 2;
  h = 3;

  printf("h = <%d, %d, %d>\n", h, h<:1:>, h<:2:>);

  *p = 4;
  *p = 5;
  *p = 6;

  printf("h = <%d, %d, %d>\n", h, h<:1:>, h<:2:>);

  h = 7;

  printf("h = <%d, %d, %d>\n", h, h<:1:>, h<:2:>);
}

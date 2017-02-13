/*
 * tests/pointer.hc
 * Ryan Mallon (2005)
 *
 * Pointer test
 *
 */
#include <print.h>

int main() {
  var int a, *p;

  a = 10;
  p = &a;
  print_num(a);
  print_num(*p);
  print_sep();

  *p = 20;
  print_num(a);
  print_num(*p);
  print_sep();

  a = 30;
  print_num(a);
  print_num(*p);
}

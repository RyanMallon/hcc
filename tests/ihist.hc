/*
 * tests/ihist.hc
 * Ryan Mallon (2006)
 *
 * Test indexed scalar history variables
 *
 */

#include <print.h>

func void local();
var int gh<:3:>;

void local() {
  var int i, lh<:3:>;

  for(i = 0; i < 4; i = i + 1) {
    lh = i + 1;
  }

  for(i = 0; i < 4; i = i + 1) {
    print_num(lh<:i:>);
  }
}

void main() {
  var int i;

  for(i = 0; i < 4; i = i + 1) {
    gh = i + 1;
  }

  for(i = 0; i < 4; i = i + 1) {
    print_num(gh<:i:>);
  }

  print_sep();
  local();
}

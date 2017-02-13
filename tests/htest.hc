var int a<:2:>, *p;

#include <io.h>
#include <history.h>
#include <history.hc>

void main() {

  a = 1;
  a = 2;
  a = 3;

  print_num(a);
  print_num(a<:1:>);
  print_num(a<:2:>);

}

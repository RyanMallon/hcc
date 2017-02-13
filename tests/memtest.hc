var int a[4], b[4];

#include <memory.h>
#include <memory.hc>

void main() {
  a[0] = 1;
  a[1] = 2;
  a[2] = 3;
  a[3] = 4;

  memcpy([int *]&b, [int *]&a, 4);

}

#include <print.h>

func void foo(int **p);
func void bar(int *p);

var int a[5];

void foo(int **p) {
  *p = *p + 1;
}

void bar(int *p) {
  *p = 30;
}

void main() {
  var int *p, **dp;

  a[0] = 10;
  a[1] = 20;
  a[2] = 30;

  p = &a;
  dp = &p;

  printf("dp = %p, p = %p, *p = %d\n", dp, p, *p);
  foo(dp);
  printf("dp = %p, p = %p, *p = %d\n", dp, p, *p);

}

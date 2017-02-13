/*
 * tests/array.hc
 * Ryan Mallon (2006)
 *
 * Test arrays
 *
 */
#include <print.h>

func void global();

var int g[10];

void global() {
  var int i;

  /* Global array */
  printf("Global: ");
  for(i = 0; i < 10; i = i + 1) {
    g[i] = i + 1;
    printf("%d, ", g[i]);
  }
  printf("\n");
}

void main() {
  var int a[10], i;

  /* Local array */
  printf("Local: ");
  for(i = 0; i < 10; i = i + 1) {
    a[i] = i + 1;
    printf("%d, ", a[i]);
  }
  printf("\n");

  global();
}

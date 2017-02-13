/*
 * tests/fib.hc
 * Ryan Mallon (2005)
 *
 * Recursive Fibonnaci series example
 *
 */
#include <print.h>

func int fib(int n);

int fib(int n) {
  var int n1, n2;

  if(n == 1 || n == 2) {
    return 1;
  }

  n1 = fib(n - 1);
  n2 = fib(n - 2);

  return n1 + n2;
}

int main() {
  var int i;
  var int fib_num;

  for(i = 1; i < 10; i = i + 1) {
    fib_num = fib(i);
    print_num(fib_num);
  }
}

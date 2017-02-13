/*
 * tests/fib-hist.hc
 * Ryan Mallon (2005)
 *
 * History variable Fibonacci series example
 *
 */
func void print_num(int num);
func int get_num();

int main() {
  var int i, t1, t2, fib<:1:>;

  fib = 1;
  print_num(fib);
  fib = 1;
  print_num(fib);

  for(i = 0; i < 10; i = i + 1) {

    t1 = fib;
    t2 = fib<:1:>;

    fib = t1 + t2;

    print_num(fib);
  }
}

/*
 * tests/printf.hc
 * Ryan Mallon (2006)
 *
 * Varidic function test
 *
 */
func void printf(...);

void main() {
  var int foo;

  foo = 10;

  printf("foo = %d\n", foo);
}

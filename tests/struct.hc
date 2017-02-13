/*
 * tests/struct.hc
 * Ryan Mallon (2006)
 *
 * Test structs
 *
 */
#include <print.h>

var struct {
  int a;
  int b;
} test_t;

void main() {

  var int *a;

  var test_t test1;
  var test_t *test_ptr;

  test1.a = 10;
  test1.b = 20;

  test_ptr = &test1;
  test_ptr->a = 30;

  printf("test1 = {%d, %d}, test_ptr = {%d, %d}\n",
	 test1.a, test1.b, test_ptr->a, test_ptr->b);

}

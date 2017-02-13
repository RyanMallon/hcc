/*
 * tests/iwise.plo
 * Ryan Mallon (2006)
 *
 * Index-wise test program
 *
 */
#include <print.h>

void main() {
  var int a[3]<:2:>;

    a[1] = 1;
    a[1] = 2;
    a[1] = 3;

    printf("a[1] = <%d, %d, %d>\n", a[1], a[1]<:1:>, a[1]<:2:>);

}

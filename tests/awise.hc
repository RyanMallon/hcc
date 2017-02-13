/*
 * awise.hc
 * Ryan Mallon (2006)
 *
 * Test program for array-wise history
 *
 */
#include <print.h>

void main() {
   var int b, a<:2:>[3];

   a[2] = 0; a[2] = 0; a[2] = 0;
   a[1] = 0; a[1] = 0; a[1] = 0;
   a[0] = 1; a[0] = 2; a[0] = 3;

   printf("a[0] = <%d, %d, %d>\n", a[0], a<:1:>[0], a<:2:>[0]);

   a[0] = 4;
   a[0] = 5;

   printf("a[0] = <%d, %d, %d>\n", a[0], a<:1:>[0], a<:2:>[0]);

   printf("a = <[%d, %d, %d], ",
	  a[0], a[1], a[2]);
   printf("[%d, %d, %d], ",
	  a<:1:>[0], a<:1:>[1], a<:1:>[2]);
   printf("[%d, %d, %d]>\n",
	  a<:2:>[0], a<:2:>[1], a<:2:>[2]);

   a[0] = 10;
   a[1] = 20;
   a[2] = 30;

   printf("a = <[%d, %d, %d], ",
	  a[0], a[1], a[2]);
   printf("[%d, %d, %d], ",
	  a<:1:>[0], a<:1:>[1], a<:1:>[2]);
   printf("[%d, %d, %d]>\n",
	  a<:2:>[0], a<:2:>[1], a<:2:>[2]);

}

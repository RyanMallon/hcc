/*
 * perform/aw-control.hc
 * Ryan Mallon (2006)
 *
 * Test performance of normal arrays. Used as a control for aw-assign.hc
 *
 */
#include <timer.h>

void main() {
  var int loops, i, j, a[SIZE];

  /*
   * Loop 1000 times. The calculation is necessary since HCC can't deal
   * with constant values larger than 4095 when targeting the Sparc
   */
  loops = 1000;
  loops = loops * 1000;

  /* Repeatedly store values */
  start_timer();
  for(i = 0; i < loops; i = i + 1) {
    for(j = 0; j < SIZE; j++) {
      a[j] = 1;
    }
  }
  stop_timer();
  print_time();
}

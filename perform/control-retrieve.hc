/*
 * perform/control-retrieve.hc
 * Ryan Mallon (2006)
 *
 * Test performance of normal variables retrieval time
 *
 */
#include <timer.h>

void main() {
  var int loops, i, a, v;

  /*
   * Loop 1000 times. The calculation is necessary since HCC can't deal
   * with constant values larger than 4095 when targeting the Sparc
   */
  loops = 1000;
  loops = loops * 1000;

  /* Repeatedly store values in the history variable */
  start_timer();
  for(i = 0; i < loops; i = i + 1) {
    a = v;
  }
  stop_timer();
  print_time();
}

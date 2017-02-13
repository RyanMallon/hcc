/*
 * primitive.hc
 * Ryan Mallon (2006)
 *
 */

#include <timer.h>
#include <history.h>
#include <history-nc.hc>

void main() {
  var int i, a, *p;

  p = &a;

  start_cycle_count();
  start_timer();

  /* Timed section */
  for(i = 0; i < 1000; i = i + 1) {
    a = 0;
    *p = 1;
  }

  stop_timer();
  stop_cycle_count();

}

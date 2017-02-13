/*
 * perform/assign-size.hc
 * Ryan Mallon (2006)
 *
 */
#include <timer.h>

void main() {
  var int i, loops, h<:1:>;

#ifndef NO_LOOP
  loops = 1000;
  loops = loops * 1000;

  start_timer();
  for(i = 0; i < loops; i = i + 1) {
#endif /* NO_LOOP */

    h = 1;

#if (ASSIGNS > 1)
    h = 1;
#endif

#if (ASSIGNS > 2)
    h = 1; h = 1;
#endif

#if (ASSIGNS > 4)
    h = 1; h = 1; h = 1; h = 1;
#endif

#if (ASSIGNS > 8)
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
#endif

#if (ASSIGNS > 16)
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
#endif

#if (ASSIGNS > 32)
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
#endif

#if (ASSIGNS > 64)
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
#endif

#if (ASSIGNS > 128)
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
#endif

#if (ASSIGNS > 256)
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
    h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1; h = 1;
#endif

#ifndef NO_LOOP
  }
  stop_timer();
  print_time();
#endif /* NO_LOOP */


}

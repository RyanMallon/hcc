/*
 * rtlib/timer.c
 * Ryan Mallon (2006)
 *
 */
#include <stdio.h>
#include <time.h>

static clock_t timer, elapsed;

void start_timer(void) {
  timer = clock();
}

void stop_timer(void) {
  clock_t stop_time;

  stop_time = clock();
  elapsed = stop_time - timer;
}

void print_time(void) {
  printf("Elapsed time = %d tics\n", elapsed);
}

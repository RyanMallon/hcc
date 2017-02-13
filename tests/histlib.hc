/*
 * histlib.hc
 *
 * Version of the history library written in hc, used for
 * performance tests.
 *
 * Cut and paste the necessary functions and their headers into a
 * source file to use them. The builtin history library needs to be
 * disabled in the preferences dialog of the vm.
 *
 */
func int hist_iws_load(int index, int depth, int size);
func int hist_iws_store(int size, int index, int depth, int value);

func int hist_init_iw(int addr, int ptr_addr, int elements);
func int hist_iwf_load();
func int hist_iwf_store(int index, int *ptr_addr, int size, int value);

int hist_iws_load(int index, int depth, int size) {
   var int *addr;

   addr = index + (depth * size);
   return *addr;
}

int hist_iws_store(int size, int index, int depth, int value) {
  var int last_depth, i, *memAddr, *valAddr;

   last_depth = index + (size * depth);

   /* Shuffle O(d) */
   for(i = last_depth; i >= index; i = i - size) {
      memAddr = i;
      valAddr = i - size;
      *memAddr = *valAddr;
   };

   memAddr = index;
   *memAddr = value;
}

int hist_init_iw(int addr, int ptr_addr, int elements) {
  var int i, *memAddr;

  for(i = 0; i < elements; i = i + 1) {
    memAddr = ptr_addr + (i * 4);
    *memAddr = addr + (i * 4);
  };
}

int hist_iwf_store(int index, int *ptr_addr, int size, int value) {
  var int *oldest;

  oldest = *ptr_addr - size;
  if(oldest < index) {
    oldest = ptr_addr - size;
  };

  *oldest = value;
  *ptr_addr = oldest;

}

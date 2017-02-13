var int i, count, a[100]<:1:>;

func int start_timer();
func int stop_timer();

func int hist_iws_store(int size, int index, int depth, int value);

int hist_iws_store(int size, int index, int depth, int value) {
  var int last_depth, *i, *memAddr;

   last_depth = index + (size * depth);

   /* Shuffle O(d) */
   for(i = last_depth; i >= index; i = i - size) {
     *i = *(i - size);
   };

   memAddr = index;
   *memAddr = value;
}

int main() {

  start_timer();

  for(count = 0; count < 1000; count = count + 1) {
    for(i = 0; i < 100; i = i + 1) {
      a[i] = i;
    };
  };

  stop_timer();
}

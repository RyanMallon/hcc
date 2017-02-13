var int i, count, a[100];

func int start_timer();
func int stop_timer();

func int hist_init_iw(int addr, int ptr_addr, int elements);
func int hist_iwf_store(int index, int *ptr_addr, int size, int value);

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

int main() {

  start_timer();

  for(count = 0; count < 1000; count = count + 1) {
    for(i = 0; i < 100; i = i + 1) {
      a[i] = i;
    };
  };

  stop_timer();

}

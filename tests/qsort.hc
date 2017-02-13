/*
 * Quicksort example
 */
var int a[8];

func void print_num(int num);
func void init_array();
func void print_array();
func void swap(int *x, int *y);
func void quicksort(int left, int right);

void init_array() {
  a[0] = 2;
  a[1] = 5;
  a[2] = 7;
  a[3] = 6;
  a[4] = 1;
  a[5] = 4;
  a[6] = 3;
  a[7] = 8;
}

void swap(int *x, int *y) {
  var int temp;

  temp = *x;
  *x = *y;
  *y = temp;
}

void print_array() {
  var int k;

  for(k = 0; k < 8; k = k + 1) {
    print_num(a[k]);
  }
}

void quicksort(int left, int right) {
  var int temp, pivot, i, j;

  pivot = a[right];
  i = left - 1;
  j = right;

  if(right > left) {

    do {

      do {
	i = i + 1;
      } while(a[i] < pivot);

      do {
	j = j - 1;
      } while(a[j] > pivot);

      temp = a[i];
      a[i] = a[j];
      a[j] = temp;

    } while(j > i);

    a[j] = a[i];
    a[i] = pivot;
    a[right] = temp;

    quicksort(left, i - 1);
    quicksort(i + 1, right);

  }

}

void main() {
   init_array();
   quicksort(0, 7);
   print_array();
}

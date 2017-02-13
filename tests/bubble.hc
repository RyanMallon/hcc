/*
 * bubble.hc
 * Ryan Mallon (2005)
 *
 * Bubble sort example
 */
var int a[8];

func void bubblesort();
func void swap(int *x, int *y);
func void init_array();
func void print_array();
func void print_num(int num);
func void print_sep();


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


void print_array() {
  var int k;

  for(k = 0; k < 8; k = k + 1) {
    print_num(a[k]);
  }
}

void swap(int *x, int *y) {
  var int temp;

  temp = *x;
  *x = *y;
  *y = temp;
}


void bubblesort() {

  var int i, j;

  for(i = 7; i >= 0; i = i - 1) {
    for(j = 0; j < i; j = j + 1) {
      if(a[j] > a[j + 1]) {
	swap(&a[j], &a[j + 1]);
      }
    }
  }
}


void main() {
  init_array();
  print_array();

  bubblesort();

  print_sep();
  print_array();
}

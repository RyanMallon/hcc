func void print_num(int num);
func void init();
func void print();
func void swap(int *x, int *y);
func void reverse();

var int a[6];

void init() {
  var int i;

  for(i = 0; i < 6; i = i + 1) {
    a[i] = i + 1;
  }
}

void print() {
  var int j;

  for(j = 0; j < 6; j = j + 1) {
    print_num(a[j]);
  }
}

void reverse() {
  var int k;

  for(k = 0; k < 3; k = k + 1) {
    swap(&a[k], &a[5 - k]);
  }
}

void swap(int *x, int *y) {
  var int t;

  t = *x;
  *x = *y;
  *y = t;
}


void main() {
  init();
  print();

  reverse();

  print();

}

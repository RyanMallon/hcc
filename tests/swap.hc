func int swap(int *a, int *b);

int swap(int *a, int *b) {
  var int temp;

  temp = *a;
  *a = *b;
  *b = temp;
}

/*

void main() {
  var int x, y;

  swap(&x, &y);

}

*/

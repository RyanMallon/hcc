func void print_num(int num);
func void foo(int *a, int *b);

void foo(int *a, int *b) {
     var int t;

     t = *a;
     *a = *b;
     *b = t;
}

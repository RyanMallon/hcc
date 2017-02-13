var int a;
var char b, c;

#include<io.h>

func int foo(int a);
func int bar(int b);

int foo(int a) {
   print_num(a);
}


int bar(int b) {
    print_num(b);
}

void main() {
  foo(10);
  foo(20);
}

var int x<:2:>;
func void print_num(int num);

int main() {

  x = 1;
  x = 2;
  x = 3;

  atomic {
    x = 4;
    atomic {
      x = 5;
      x = 6;
    }
    x = 7;
  }

  print_num(x);
  print_num(x<:1:>);
  print_num(x<:2:>);

}

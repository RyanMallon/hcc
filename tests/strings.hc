var string str1, str2;
func void print_str(string str);

void main() {

  str1 = "foo";
  str2 = "bar";

  if(str1 == str2) {
    print_str(str1 + str2);
  }

}

/*
 * tests/primhist.hc
 * Ryan Mallon (2006)
 *
 * Test local and global primitive history variables
 *
 */
#include <print.h>

func void local();
var int gh<:3:>;

void local() {
  var int lh<:3:>;

  lh = 1;
  printf("lh = <%d>\n", lh);

  lh = 2;
  printf("lh = <%d, %d>\n", lh, lh<:1:>);

  lh = 3;
  printf("lh = <%d, %d, %d>\n", lh, lh<:1:>, lh<:2:>);

  lh = 4;
  printf("lh = <%d, %d, %d, %d>\n", lh, lh<:1:>, lh<:2:>, lh<:3:>);

}

int main() {
  gh = 1;
  printf("gh = <%d>\n", gh);

  gh = 2;
  printf("gh = <%d, %d>\n", gh, gh<:1:>);

  gh = 3;
  printf("gh = <%d, %d, %d>\n", gh, gh<:1:>, gh<:2:>);

  gh = 4;
  printf("gh = <%d, %d, %d, %d>\n", gh, gh<:1:>, gh<:2:>, gh<:3:>);

  local();
}

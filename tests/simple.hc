#include <print.h>

int main() {
   var int a<:2:>[3];

   a[0] = 1;
   printf("a[0] = <%d, %d, %d>\n", a[0], a<:1:>[0], a<:2:>[0]);

   a[0] = 2;
   printf("a[0] = <%d, %d, %d>\n", a[0], a<:1:>[0], a<:2:>[0]);

   a[0] = 3;
   printf("a[0] = <%d, %d, %d>\n", a[0], a<:1:>[0], a<:2:>[0]);
}

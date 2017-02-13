var int dummy;

int main() {
   var int x, y, z, w, u, v;

   v = 1;
   z = v + 1;
   x = z * v;
   y = x * 2;
   w = x + z * y;
   u = z + 2;
   v = u + w + y;

   return v * u;
}

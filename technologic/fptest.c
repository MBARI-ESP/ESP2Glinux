#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void dumphex (void *adr, size_t sz)
{
  char *ch = adr;
  printf (" =");
  do
    printf (" %02x", *ch++ & 0xff);
  while (--sz);
  puts ("");
}

int main (int argc, char **argv)
{
  if (argc > 1) {
    double d = atof (argv[1]);
    float f, f2, f3;
    int i, i2, i3;
    printf ("atof(%s) = %g", argv[1], d);
    dumphex (&d, sizeof(d));
    sscanf (argv[1], "%f", &f);
    printf ("sscanf(%s, \"%%f\", &f) assigns f = %g", argv[1], f);
    dumphex (&f, sizeof(f));
    printf ("(int)%g = %d", f, i=(int)f);
    dumphex (&i, sizeof(i));
    printf ("fabs(%g) = %g", f, f2=fabs(f));
    dumphex (&f2, sizeof(f2));
    printf ("ceilf(%g) = %d", f, i2=ceilf(f));
    dumphex (&i2, sizeof(i2));
    printf ("ceil(%g) = %d", f, i2=ceil((double)f));
    dumphex (&i3, sizeof(i3));
    printf ("exp(%g) = %g", f, f3=exp((double)f));
    dumphex (&f3, sizeof(f3));
    return 0;
  }
  return 2;
}

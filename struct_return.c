#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <mmintrin.h>

typedef float  v4sf __attribute__ ((vector_size(16)));
typedef struct vfloat vfloat;

struct vfloat {
    union {
        v4sf       v;
        float      f[4];
        struct {
            float  x;
            float  y;
            float  z;
            float  w;
        };
    };
};

struct a {
  int i;
  vfloat d;  
};

struct a f(struct a x)
{
   struct a r = x;
   return r;
}

// ucomiss
// bool NEQ

bool EQ(struct a a, struct a b)
{
  if((a.i == b.i) && (a.d.x == b.d.x)) return true; // no we want to do the vector thing
  return false;
}

int main(void)
{
  struct a x = { .i = 11, .d.f = { 12., .2, .3, .4} };
  struct a y = f(x);
  if(EQ(y,x)) {
    printf("%d\n", y.i);
  }
  return 0;
}

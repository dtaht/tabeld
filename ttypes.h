#ifndef T_TYPES
#define T_TYPES

#include <stdint.h>
#include <stdbool.h>

/* The default types are too verbose to type */

typedef unsigned char byte;
typedef unsigned char u8;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

/* Dealing with vector based types is a PITA */

/* What we want is an anonymous union */

typedef float  v4sf __attribute__ ((vector_size(16)));
typedef struct vfloat vfloat;

struct vfloat {
  union {
    v4sf       v;
    float      f[4];
    double     d[2];
    u8         b[16];
    struct {
      float  x;
      float  y;
      float  z;
      float  w;
    };
  };
};


typedef u32 v4i __attribute__((vector_size(16)));

typedef struct u128 u128;

struct u128 {
  union {
    v4i       v;
    u32       f[4];
    u64       d[2];
    u8        b[16];
    struct {
      u32 x;
      u32 y;
      u32 z;
      u32 w;
    };
  };
};


static inline v4sf add(v4sf u, v4sf v) {
    return u + v;
}

// up to 8 transferred only one returned

// typedef __m128   s128;

#endif

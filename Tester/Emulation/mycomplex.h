
#pragma once
typedef struct 
{
  float re, im;
} cf;
#ifndef M_PI
#define M_PI 3.141592653589793238462643383279502884197169399375105821
#endif
#define CF(r, i) ((cf){r, i})
static inline float sqr(float y) { return y * y; }
static inline cf cadd(cf a, cf b) { return CF(a.re + b.re, a.im + b.im); }
static inline float csqr(cf a) { return a.re * a.re + a.im * a.im; }
static inline cf cconj(cf a) { return CF(a.re, -a.im);  }
static inline cf ccmul(cf a, cf b) { return CF(a.re * b.re - a.im * b.im, a.im * b.re + b.im * a.re); }
static inline float ccmulre(cf a, cf b) { return a.re * b.re - a.im * b.im; }
static inline cf cfmul(cf a, float b) { return CF(a.re * b, a.im * b); }

#define C0 CF(0, 0)

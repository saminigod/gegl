static const char* random_cl_source =
"/* This file is part of GEGL                                                  \n"
" *                                                                            \n"
" * GEGL is free software; you can redistribute it and/or                      \n"
" * modify it under the terms of the GNU Lesser General Public                 \n"
" * License as published by the Free Software Foundation; either               \n"
" * version 3 of the License, or (at your option) any later version.           \n"
" *                                                                            \n"
" * GEGL is distributed in the hope that it will be useful,                    \n"
" * but WITHOUT ANY WARRANTY; without even the implied warranty of             \n"
" * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU          \n"
" * Lesser General Public License for more details.                            \n"
" *                                                                            \n"
" * You should have received a copy of the GNU Lesser General Public           \n"
" * License along with GEGL; if not, see <http://www.gnu.org/licenses/>.       \n"
" *                                                                            \n"
" * Copyright 2013 Carlos Zubieta (czubieta.dev@gmail.com)                     \n"
" */                                                                           \n"
"                                                                              \n"
"/* XXX: this file should be kept in sync with gegl-random. */                 \n"
"#define XPRIME     103423                                                     \n"
"#define YPRIME     101359                                                     \n"
"#define NPRIME     101111                                                     \n"
"                                                                              \n"
"#define RANDOM_DATA_SIZE (15083+15091+15101)                                  \n"
"#define PRIME_SIZE 533u                                                       \n"
"                                                                              \n"
"inline uint _gegl_cl_random_int  (__global const int  *cl_random_data,        \n"
"                                  __global const long *cl_random_primes,      \n"
"                                  int seed, int x, int y, int z, int n);      \n"
"                                                                              \n"
"uint gegl_cl_random_int          (__global const int  *cl_random_data,        \n"
"                                  __global const long *cl_random_primes,      \n"
"                                  int seed, int x, int y, int z, int n);      \n"
"                                                                              \n"
"int gegl_cl_random_int_range     (__global const int  *cl_random_data,        \n"
"                                  __global const long *cl_random_primes,      \n"
"                                  int seed, int x, int y, int z, int n,       \n"
"                                  int min, int max);                          \n"
"                                                                              \n"
"float gegl_cl_random_float       (__global const int  *cl_random_data,        \n"
"                                  __global const long *cl_random_primes,      \n"
"                                  int seed, int x, int y, int z, int n);      \n"
"                                                                              \n"
"float gegl_cl_random_float_range (__global const int  *cl_random_data,        \n"
"                                  __global const long *cl_random_primes,      \n"
"                                  int seed, int x, int y, int z, int n,       \n"
"                                  float min, float max);                      \n"
"                                                                              \n"
"inline uint                                                                   \n"
"_gegl_cl_random_int (__global const int  *cl_random_data,                     \n"
"                     __global const long *cl_random_primes,                   \n"
"                     int                 seed,                                \n"
"                     int                 x,                                   \n"
"                     int                 y,                                   \n"
"                     int                 z,                                   \n"
"                     int                 n)                                   \n"
"{                                                                             \n"
"  unsigned long idx = x * XPRIME +                                            \n"
"                      y * YPRIME * XPRIME +                                   \n"
"                      n * NPRIME * YPRIME * XPRIME;                           \n"
"#define ROUNDS 3                                                              \n"
"    /* 3 rounds gives a reasonably high cycle for */                          \n"
"    /*   our synthesized larger random set. */                                \n"
"  unsigned long seed_idx = seed % (PRIME_SIZE - 1 - ROUNDS);                  \n"
"  int prime0 = cl_random_primes[seed_idx],                                    \n"
"      prime1 = cl_random_primes[seed_idx+1],                                  \n"
"      prime2 = cl_random_primes[seed_idx+2];                                  \n"
"  int r0 = cl_random_data[idx % prime0],                                      \n"
"      r1 = cl_random_data[prime0 + (idx % (prime1))],                         \n"
"      r2 = cl_random_data[prime0 + prime1 + (idx % (prime2))];                \n"
"  return r0 ^ r1 ^ r2;                                                        \n"
"}                                                                             \n"
"                                                                              \n"
"uint                                                                          \n"
"gegl_cl_random_int (__global const int  *cl_random_data,                      \n"
"                    __global const long *cl_random_primes,                    \n"
"                    int                 seed,                                 \n"
"                    int                 x,                                    \n"
"                    int                 y,                                    \n"
"                    int                 z,                                    \n"
"                    int                 n)                                    \n"
"{                                                                             \n"
"  return _gegl_cl_random_int (cl_random_data, cl_random_primes,               \n"
"                              seed, x, y, z, n);                              \n"
"}                                                                             \n"
"                                                                              \n"
"int                                                                           \n"
"gegl_cl_random_int_range (__global const int  *cl_random_data,                \n"
"                          __global const long *cl_random_primes,              \n"
"                          int                 seed,                           \n"
"                          int                 x,                              \n"
"                          int                 y,                              \n"
"                          int                 z,                              \n"
"                          int                 n,                              \n"
"                          int                 min,                            \n"
"                          int                 max)                            \n"
"{                                                                             \n"
"  uint ret = _gegl_cl_random_int (cl_random_data, cl_random_primes,           \n"
"                                  seed, x, y, z, n);                          \n"
"  return (ret % (max-min)) + min;                                             \n"
"}                                                                             \n"
"                                                                              \n"
"                                                                              \n"
"#define G_RAND_FLOAT_TRANSFORM  0.00001525902189669642175f                    \n"
"                                                                              \n"
"float                                                                         \n"
"gegl_cl_random_float (__global const int  *cl_random_data,                    \n"
"                      __global const long *cl_random_primes,                  \n"
"                      int                 seed,                               \n"
"                      int                 x,                                  \n"
"                      int                 y,                                  \n"
"                      int                 z,                                  \n"
"                      int                 n)                                  \n"
"{                                                                             \n"
"  uint u = _gegl_cl_random_int (cl_random_data, cl_random_primes,             \n"
"                                seed, x, y, z, n);                            \n"
"  return (u & 0xffff) * G_RAND_FLOAT_TRANSFORM;                               \n"
"}                                                                             \n"
"                                                                              \n"
"float                                                                         \n"
"gegl_cl_random_float_range (__global const int  *cl_random_data,              \n"
"                            __global const long *cl_random_primes,            \n"
"                            int                 seed,                         \n"
"                            int                 x,                            \n"
"                            int                 y,                            \n"
"                            int                 z,                            \n"
"                            int                 n,                            \n"
"                            float               min,                          \n"
"                            float               max)                          \n"
"{                                                                             \n"
"  float f = gegl_cl_random_float (cl_random_data, cl_random_primes,           \n"
"                                  seed, x, y, z, n);                          \n"
"  return f * (max - min) + min;                                               \n"
"}                                                                             \n"
;


/* https://github.com/simd-everywhere/simde */


#ifndef _SIMDE_H_
#define _SIMDE_H_


// These add support for functions not covered by 'simd-everywhere'

//#define _tzcnt_u64(x) __builtin_ctzll(x)
//#define _lzcnt64(x)  __builtin_clzll(x)
//#define _lzcnt_u64(x)  __builtin_clzll(x)
//#define _popcnt64(x)  __builtin_popcountll(x)
#define __popcnt64(x)  __builtin_popcountll(x)

//#define _mm_popcnt_u64(x) __builtin_popcountll(x)
//static uint64_t _blsr_u64(uint64_t x) { return x & (x - 1); }




/*
#include "/autodetect/check.h"
#include "/autodetect/simde-features.h"
#include "/autodetect/simde-f16.h"
#include "/autodetect/simde-diagnostic.h"
#include "/autodetect/simde-align.h"
#include "/autodetect/simde-math.h"
#include "/autodetect/debug-trap.h"
#include "/autodetect/simde-aes.h"
#include "/autodetect/simde-align.h"
#include "/autodetect/simde-constify.h"
#include "/autodetect/simde-complex.h"
*/

//#define SIMDE_X86_AVX_ENABLE_NATIVE_ALIASES

#define SIMDE_ENABLE_NATIVE_ALIASES
#define SIMDE_ENABLE_OPENMP            // add   -fopenmp-simd   to makefile

/* select features you need to support */


#include "./simde/zp7.h"                      // adds _pext_u64 & _pdep_u64 support  
                                              // add -DHAS_POPCNT to makefile if cpu has popcount
											  
//#include "./simde/avx.h"
#include "./simde/avx2.h"        
//#include "./simde/avx512.h"
//#include "./simde/clmul.h"
//#include "./simde/f16c.h"
//#include "./simde/fma.h"
//#include "./simde/gfni.h"
//#include "./simde/mmx.h"
//#include "./simde/sse.h"
//#include "./simde/sse2.h"
//#include "./simde/sse3.h"
//#include "./simde/sse4.1.h"
//#include "./simde/sse4.2.h"    
//#include "./simde/ssse3.h"
//#include "./simde/svml.h"
//#include "./simde/xop.h"
//#include "./simde/sve.h"


#endif















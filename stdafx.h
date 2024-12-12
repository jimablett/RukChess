// stdafx.h

#pragma once

#ifndef STDAFX_H
#define STDAFX_H

#pragma warning(disable: 4100)  // Unreferenced formal parameter

#include "targetver.h"

#ifdef _WIN32
#include <windows.h>            // Master include file for Windows applications (includes additional header files; see below)
#else
#include <unistd.h>
#endif

//#include <winnt.h>              // SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, GROUP_AFFINITY
//#include <sysinfoapi.h>         // GetLogicalProcessorInformationEx()
//#include <systemtopologyapi.h>  // GetNumaNodeProcessorMaskEx()
//#include <processtopologyapi.h> // SetThreadGroupAffinity()
//#include <winbase.h>            // SetThreadAffinityMask()
//#include <processthreadsapi.h>  // GetCurrentThread()
//#include <synchapi.h>           // Sleep()
//#include <profileapi.h>         // QueryPerformanceCounter(), QueryPerformanceFrequency()

#include <stdio.h>              // _IONBF, printf(), scanf_s(), fopen_s(), fseek(), ftell(), fclose(), fprintf(), fgets(), sprintf_s()
#include <stdlib.h>             // _countof(), atoi(), strtoull(), malloc(), realloc(), calloc(), free(), qsort()
#include <string.h>             // strcmp(), strncmp(), strchr(), strstr(), strcpy_s()
#if !defined(__arm__) && !defined(__aarch64__)
#include <sys/timeb.h>          // _timeb, _ftime_s()
#else
#include <sys/time.h> 
#endif
#ifdef _WIN32
#include <process.h>            // _beginthread(), _endthread()
#else
#include "process.h"
#endif
#include <omp.h>                // Open MP
#ifdef USE_SIMDE
#include "simde.h"
#elif defined(__linux__)
#include <immintrin.h>          // _pdep_u64(), _pext_u64()
#else
#include <intrin.h>             // __popcnt64(), _BitScanForward64(), _BitScanReverse64(), _mm_prefetch()
#include <immintrin.h>          // _pdep_u64(), _pext_u64()
#endif

#include <limits.h>             // INT_MAX
#include <float.h>              // FLT_MAX
#include <math.h>               // round(), pow(), log()
#include <assert.h>             // assert()

#endif // !STDAFX_H

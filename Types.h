// Types.h

#pragma once

#ifndef TYPES_H
#define TYPES_H

#include "Def.h"

#define BOOL                int

#define FALSE               0
#define TRUE                1

#define INF                 10000

#ifdef _WIN32
typedef __int8              I8;
typedef __int16             I16;
typedef __int32             I32;
typedef __int64             I64;

typedef unsigned __int8     U8;
typedef unsigned __int16    U16;
//typedef unsigned __int32    U32;
typedef unsigned __int64    U64;

#else

#include <cstdint>

typedef int8_t              I8;
typedef int16_t             I16;
typedef int32_t             I32;
typedef int64_t             I64;

typedef uint8_t     U8;
typedef uint16_t    U16;
//typedef unsigned __int32    U32;
typedef uint64_t    U64;
#endif




#endif // !TYPES_H

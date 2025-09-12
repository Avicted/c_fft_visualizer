#ifndef REDEFINES_H
#define REDEFINES_H

#include <stddef.h>
#include <stdint.h>

#define internal static      // functions/variables with internal linkage (file-local scope)
#define local_persist static // variables inside functions that retain their value between calls
#define global static        // file-scope variables meant to act as globals within that file

// Unsigned integers
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// Signed integers
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// Pointer-sized types
typedef size_t usize;
typedef intptr_t isize;

// Other common aliases
typedef unsigned long ul;
typedef unsigned long long ull;

// Floating-poi32 types
typedef float f32;
typedef double f64;

#endif // REDEFINES_H

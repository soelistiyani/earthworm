/* --------------------------------------------------------------------
  
  Standard explicit data type to enhance portability.

  R. Banfill and D. Chavez

----------------------------------------------------------------------- */

#ifndef STD_TYPES_H
#define STD_TYPES_H

#include <stdint.h>

/* Void type */
#if defined VOID
#   undef VOID
#endif
typedef void VOID;

 /* Characters */
#if defined CHAR
#   undef CHAR
#endif
typedef char CHAR;

 /* Signed and unsigned 8 bit integers */
typedef int8_t INT8;
typedef uint8_t UINT8;

 /* 16 bit integer values */
typedef int16_t INT16;
typedef uint16_t UINT16;

 /* 32 bit integer values */
typedef int32_t INT32;
typedef uint32_t UINT32;

 /* 64 bit integers */
typedef int64_t INT64;
typedef uint64_t UINT64;

 /* 32 bit IEEE 754 Real */
typedef float REAL32;

 /* 64 bit IEEE 754 Real */
typedef double REAL64;

 /* 80 bit IEEE 754 Real */
typedef long double REAL80;

/* Boolean constants and variables ------------------------------------ */

#ifdef WIN32

/* BOOL, TRUE, and FALSE are in windef.h
 */

#include <windows.h>

#else

/* We make BOOL an int because NT (currently) has BOOL that big and some
 * code exists which looks for offsets using sizeof(BOOL) and that causes
 * problems for data portability.  This will keep things happy until such
 * time as NT changes the size of BOOL.
 */

#if defined BOOL
#   undef BOOL
#endif
typedef int BOOL;

#ifndef TRUE
#    define TRUE  ((BOOL) 1)
#endif

#ifndef FALSE
#    define FALSE ((BOOL) 0)
#endif

#endif /* WIN32 */

#endif /* STD_TYPES_H */

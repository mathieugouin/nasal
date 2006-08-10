#ifndef _NAREF_H
#define _NAREF_H

/* Rather than play elaborate and complicated games with
 * platform-dependent endianness headers, just detect the platforms we
 * support.  This list is simpler and smaller, yet still quite
 * complete. */
#if (defined(__x86_64) && defined(__linux__)) || defined(__sparcv9)
#   define NASAL_44BIT_ADDR
#elif defined(_M_X86)   || defined(i386)    || defined(__x86_64) || \
    defined(__ia64__) || defined(_M_IA64) || defined(__ARMEL__) 
# define NASAL_LE
#elif defined(__sparc) || defined(__ppc__) || defined(__mips) || \
      defined(__ARMEB__)
# define NASAL_BE
#else
# error Unrecognized CPU architecture
#endif

typedef union {
    struct naObj* obj;
    struct naStr* str;
    struct naVec* vec;
    struct naHash* hash;
    struct naCode* code;
    struct naFunc* func;
    struct naCCode* ccode;
    struct naGhost* ghost;
} naPtr;

#if defined(NASAL_44BIT_ADDR)

/* On suppoted 64 bit platforms (those where all memory returned from
 * naAlloc() is guaranteed to lie between 0 and 2^44-1) we union the
 * double with the pointer, and use fancy tricks (see data.h) to make
 * sure all pointers are stored as NaNs. */
typedef union { double num; void* ptr; } naRef;

#elif defined(NASAL_LE) || defined(NASAL_BE)

/* 32 bit layouts (and 64 bit platforms where we haven't tested the
   trick above) need endianness-dependent ordering to make sure that
   the reftag lies in the top bits of the double */
#ifdef NASAL_LE
typedef struct { naPtr ptr; int reftag; } naRefPart;
#else /* NASAL_BE */
typedef struct { int reftag; naPtr ptr; } naRefPart;
#endif

typedef union {
    double num;
    naRefPart ref;
} naRef;

#endif

#endif // _NAREF_H

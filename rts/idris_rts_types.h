#ifndef _IDRIS_RTS_TYPES_H
#define _IDRIS_RTS_TYPES_H

#include <stdint.h>

#include "idris_heap.h"
#include "idris_stats.h"
#ifdef HAS_PTHREAD
#include "idris_pthread.h"
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

// Closures
typedef enum {
    CT_CON, CT_ARRAY, CT_INT, CT_BIGINT,
    CT_FLOAT, CT_STRING, CT_STROFFSET, CT_BITS32,
    CT_BITS64, CT_PTR, CT_REF, CT_FWD,
    CT_MANAGEDPTR, CT_RAWDATA, CT_CDATA
} ClosureType;

typedef struct Hdr {
    uint8_t ty;
    uint8_t u8;
    uint16_t u16;
    uint32_t sz;
} Hdr;

typedef struct Val {
    Hdr hdr;
} Val;

typedef struct Val * VAL;

typedef struct Con {
    Hdr hdr;
    uint32_t tag;
    VAL args[0];
} Con;

typedef struct Array {
    Hdr hdr;
    VAL array[0];
} Array;

typedef struct BigInt {
    Hdr hdr;
    char big[0];
} BigInt;

typedef struct Float {
    Hdr hdr;
    double f;
} Float;

typedef struct String {
    Hdr hdr;
    size_t slen;
    char str[0];
} String;

typedef struct StrOffset {
    Hdr hdr;
    String * base;
    size_t offset;
} StrOffset;

typedef struct Bits32 {
    Hdr hdr;
    uint32_t bits32;
} Bits32;

typedef struct Bits64 {
    Hdr hdr;
    uint64_t bits64;
} Bits64;

typedef struct Ptr {
    Hdr hdr;
    void * ptr;
} Ptr;

typedef struct Ref {
    Hdr hdr;
    VAL ref;
} Ref;

typedef struct Fwd {
    Hdr hdr;
    VAL fwd;
} Fwd;

typedef struct ManagedPtr {
    Hdr hdr;
    char mptr[0];
} ManagedPtr;

typedef struct RawData {
    Hdr hdr;
    char raw[0];
} RawData;

typedef struct CDataC {
    Hdr hdr;
    CHeapItem * item;
} CDataC;

struct VM {
    int active; // 0 if no longer running; keep for message passing
                // TODO: If we're going to have lots of concurrent threads,
                // we really need to be cleverer than this!

    VAL* valstack;
    VAL* valstack_top;
    VAL* valstack_base;
    VAL* stack_max;

    CHeap c_heap;
    Heap heap;
#ifdef HAS_PTHREAD
    struct VMPthread_t* pthread;
#endif
    Stats stats;

    VAL ret;
    VAL reg1;
};

typedef struct VM VM;

typedef void*(*func)(struct VM*, VAL*);

#endif

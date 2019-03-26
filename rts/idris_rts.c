#include <assert.h>
#include <errno.h>
#include <string.h>

#include "idris_rts.h"
#include "idris_gc.h"
#include "idris_utf8.h"
#include "idris_bitstring.h"
#include "getline.h"
#ifdef HAS_PTHREAD
#include "idris_pthread.h"
#else
#include "idris_single_threaded.h"
#endif

#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(COND)?1:-1]

STATIC_ASSERT(sizeof(Hdr) == 8, condSizeOfHdr);

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__DragonFly__)
#include <signal.h>
#endif

VM* init_vm(int stack_size, size_t heap_size,
            int max_threads // not implemented yet
            ) {

    VM* vm = malloc(sizeof(VM));
    STATS_INIT_STATS(vm->stats)
    STATS_ENTER_INIT(vm->stats)

    VAL* valstack = malloc(stack_size * sizeof(VAL));

    vm->active = 1;
    vm->valstack = valstack;
    vm->valstack_top = valstack;
    vm->valstack_base = valstack;
    vm->stack_max = valstack + stack_size;

    alloc_heap(&(vm->heap), heap_size, heap_size, NULL);

    c_heap_init(&vm->c_heap);

    vm->ret = NULL;
    vm->reg1 = NULL;
#ifdef HAS_PTHREAD
    vm->pthread = alloc_vm_pthread(vm, max_threads);
#else
    init_vm_single(vm);
#endif
    STATS_LEAVE_INIT(vm->stats)
    return vm;
}

VM* idris_vm(void) {
    VM* vm = init_vm(4096000, 4096000, 1);
    init_gmpalloc();
    init_nullaries();
    init_signals();

    return vm;
}

VM* get_vm(void) {
    return get_vm_impl();
}

void close_vm(VM* vm) {
    terminate(vm);
}

void init_signals(void) {
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__DragonFly__)
    signal(SIGPIPE, SIG_IGN);
#endif
}

Stats terminate(VM* vm) {
    Stats stats = vm->stats;
    STATS_ENTER_EXIT(stats)
    free(vm->valstack);
    free_heap(&(vm->heap));
    c_heap_destroy(&(vm->c_heap));
    free_vm_threaded(vm);
    // free(vm);
    // Set the VM as inactive, so that if any message gets sent to it
    // it will not get there, rather than crash the entire system.
    // (TODO: We really need to be cleverer than this if we're going to
    // write programs than create lots of threads...)
    vm->active = 0;

    STATS_LEAVE_EXIT(stats)
    return stats;
}

CData cdata_allocate(size_t size, CDataFinalizer finalizer)
{
    void * data = (void *) malloc(size);
    return cdata_manage(data, size, finalizer);
}

CData cdata_manage(void * data, size_t size, CDataFinalizer finalizer)
{
    return c_heap_create_item(data, size, finalizer);
}

void idris_requireAlloc(VM * vm, size_t size) {
    idris_requireAlloc_impl(vm, size);
}

void idris_doneAlloc(VM * vm) {
    idris_doneAlloc_impl(vm);
}

int space(VM* vm, size_t size) {
    return (vm->heap.next + size) < vm->heap.end;
}

void* idris_alloc(size_t size) {
    RawData * cl = (RawData*) allocate(sizeof(*cl)+size, 0);
    SETTY(cl, CT_RAWDATA);
    return (void*)cl->raw;
}

void* idris_realloc(void* old, size_t old_size, size_t size) {
    void* ptr = idris_alloc(size);
    memcpy(ptr, old, old_size);
    return ptr;
}

void idris_free(void* ptr, size_t size) {
}

void * allocate(size_t sz, int lock) {
    return iallocate(get_vm(), sz, lock);
}

void* iallocate(VM * vm, size_t isize, int outerlock) {
    return iallocate_impl(vm, isize, outerlock);
}

static String * allocStr(VM * vm, size_t len, int outer) {
    String * cl = iallocate(vm, sizeof(*cl) + len + 1, outer);
    SETTY(cl, CT_STRING);
    cl->slen = len;
    return cl;
}

static VAL mkfloat(VM* vm, double val, int outer) {
    Float * cl = iallocate(vm, sizeof(*cl), outer);
    SETTY(cl, CT_FLOAT);
    cl->f = val;
    return (VAL)cl;
}

VAL MKFLOAT(VM* vm, double val) {
    return mkfloat(vm, val, 0);
}

VAL MKFLOATc(VM* vm, double val) {
    return mkfloat(vm, val, 1);
}

static VAL mkstrlen(VM* vm, const char * str, size_t len, int outer) {
    String * cl = allocStr(vm, len, outer);
    // hdr.u8 used to mark a null string
    cl->hdr.u8 = str == NULL;
    if (!cl->hdr.u8)
      memcpy(cl->str, str, len);
    return (VAL)cl;
}

VAL MKSTRlen(VM* vm, const char * str, size_t len) {
    return mkstrlen(vm, str, len, 0);
}

VAL MKSTRclen(VM* vm, char* str, size_t len) {
    return mkstrlen(vm, str, len, 1);
}

VAL MKSTR(VM* vm, const char* str) {
    return mkstrlen(vm, str, str? strlen(str) : 0, 0);
}

VAL MKSTRc(VM* vm, char* str) {
    return mkstrlen(vm, str, strlen(str), 1);
}

static char * getstroff(StrOffset * stroff) {
    return stroff->base->str + stroff->offset;
}

char* GETSTROFF(VAL stroff) {
    // Assume STROFF
    return getstroff((StrOffset*)stroff);
}

static size_t getstrofflen(StrOffset * stroff) {
    return stroff->base->slen - stroff->offset;
}

size_t GETSTROFFLEN(VAL stroff) {
    // Assume STROFF
    // we're working in char* here so no worries about utf8 char length
    return getstrofflen((StrOffset*)stroff);
}

static VAL mkcdata(VM * vm, CHeapItem * item, int outer) {
    c_heap_insert_if_needed(vm, &vm->c_heap, item);
    CDataC * cl = iallocate(vm, sizeof(*cl), outer);
    SETTY(cl, CT_CDATA);
    cl->item = item;
    return (VAL)cl;
}

VAL MKCDATA(VM* vm, CHeapItem * item) {
    return mkcdata(vm, item, 0);
}

VAL MKCDATAc(VM* vm, CHeapItem * item) {
    return mkcdata(vm, item, 1);
}

static VAL mkptr(VM* vm, void* ptr, int outer) {
    Ptr * cl = iallocate(vm, sizeof(*cl), outer);
    SETTY(cl, CT_PTR);
    cl->ptr = ptr;
    return (VAL)cl;
}

VAL MKPTR(VM* vm, void* ptr) {
    return mkptr(vm, ptr, 0);
}

VAL MKPTRc(VM* vm, void* ptr) {
    return mkptr(vm, ptr, 1);
}

VAL mkmptr(VM* vm, void* ptr, size_t size, int outer) {
    ManagedPtr * cl = iallocate(vm, sizeof(*cl) + size, outer);
    SETTY(cl, CT_MANAGEDPTR);
    memcpy(cl->mptr, ptr, size);
    return (VAL)cl;
}

VAL MKMPTR(VM* vm, void* ptr, size_t size) {
    return mkmptr(vm, ptr, size, 0);
}

VAL MKMPTRc(VM* vm, void* ptr, size_t size) {
    return mkmptr(vm, ptr, size, 1);
}

VAL MKB8(VM* vm, uint8_t bits8) {
    return MKINT(bits8);
}

VAL MKB16(VM* vm, uint16_t bits16) {
    return MKINT(bits16);
}

VAL MKB32(VM* vm, uint32_t bits32) {
    Bits32 * cl = iallocate(vm, sizeof(*cl), 1);
    SETTY(cl, CT_BITS32);
    cl->bits32 = bits32;
    return (VAL)cl;
}

VAL MKB64(VM* vm, uint64_t bits64) {
    Bits64 * cl = iallocate(vm, sizeof(*cl), 1);
    SETTY(cl, CT_BITS64);
    cl->bits64 = bits64;
    return (VAL)cl;
}

void idris_trace(VM* vm, const char* func, int line) {
    printf("At %s:%d\n", func, line);
    dumpStack(vm);
    puts("");
    fflush(stdout);
}

void dumpStack(VM* vm) {
    int i = 0;
    VAL* root;

    for (root = vm->valstack; root < vm->valstack_top; ++root, ++i) {
        printf("%d: ", i);
        dumpVal(*root);
        if (*root >= (VAL)(vm->heap.heap) && *root < (VAL)(vm->heap.end)) { printf(" OK"); }
        if (root == vm->valstack_base) { printf(" <--- base"); }
        printf("\n");
    }
    printf("RET: ");
    dumpVal(vm->ret);
    printf("\n");
}

void dumpVal(VAL v) {
    if (v==NULL) return;
    int i;
    switch(GETTY(v)) {
    case CT_INT:
        printf("%" PRIdPTR " ", GETINT(v));
        break;
    case CT_CON:
        {
            Con * cl = (Con*)v;
            printf("%d[", (int)TAG(cl));
            for(i = 0; i < CARITY(cl); ++i) {
                dumpVal(cl->args[i]);
            }
            printf("] ");
        }
        break;
    case CT_STRING:
        {
            String * cl = (String*)v;
            printf("STR[%s]", cl->str);
        }
        break;
    case CT_STROFFSET:
        {
            StrOffset * cl = (StrOffset*)v;
            printf("OFFSET[");
            dumpVal((VAL)cl->base);
            printf("]");
        }
        break;
    case CT_FWD:
        {
            Fwd * cl = (Fwd*)v;
            printf("CT_FWD ");
            dumpVal((VAL)cl->fwd);
        }
        break;
    default:
        printf("val");
    }

}

void idris_memset(void* ptr, i_int offset, uint8_t c, i_int size) {
    memset(((uint8_t*)ptr) + offset, c, size);
}

uint8_t idris_peek(void* ptr, i_int offset) {
    return *(((uint8_t*)ptr) + offset);
}

void idris_poke(void* ptr, i_int offset, uint8_t data) {
    *(((uint8_t*)ptr) + offset) = data;
}


VAL idris_peekPtr(VM* vm, VAL ptr, VAL offset) {
    void** addr = (void **)(((char *)GETPTR(ptr)) + GETINT(offset));
    return MKPTR(vm, *addr);
}

VAL idris_pokePtr(VAL ptr, VAL offset, VAL data) {
    void** addr = (void **)((char *)GETPTR(ptr) + GETINT(offset));
    *addr = GETPTR(data);
    return MKINT(0);
}

VAL idris_peekDouble(VM* vm, VAL ptr, VAL offset) {
    return MKFLOAT(vm, *(double*)((char *)GETPTR(ptr) + GETINT(offset)));
}

VAL idris_pokeDouble(VAL ptr, VAL offset, VAL data) {
    *(double*)((char *)GETPTR(ptr) + GETINT(offset)) = GETFLOAT(data);
    return MKINT(0);
}

VAL idris_peekSingle(VM* vm, VAL ptr, VAL offset) {
    return MKFLOAT(vm, *(float*)((char *)GETPTR(ptr) + GETINT(offset)));
}

VAL idris_pokeSingle(VAL ptr, VAL offset, VAL data) {
    *(float*)((char *)GETPTR(ptr) + GETINT(offset)) = GETFLOAT(data);
    return MKINT(0);
}

void idris_memmove(void* dest, void* src, i_int dest_offset, i_int src_offset, i_int size) {
    memmove((char *)dest + dest_offset, (char *)src + src_offset, size);
}

VAL idris_castIntStr(VM* vm, VAL i) {
    int x = (int) GETINT(i);
    String * cl = allocStr(vm, 16, 0);
    cl->slen = sprintf(cl->str, "%d", x);
    return (VAL)cl;
}

VAL idris_castBitsStr(VM* vm, VAL i) {
    String * cl;
    ClosureType ty = GETTY(i);

    switch (ty) {
    case CT_INT: // 8/16 bits
        // max length 16 bit unsigned int str 5 chars (65,535)
        cl = allocStr(vm, 6, 0);
        cl->slen = sprintf(cl->str, "%" PRIu16, (uint16_t)GETBITS16(i));
        break;
    case CT_BITS32:
        // max length 32 bit unsigned int str 10 chars (4,294,967,295)
        cl = allocStr(vm, 11, 0);
        cl->slen = sprintf(cl->str, "%" PRIu32, GETBITS32(i));
        break;
    case CT_BITS64:
        // max length 64 bit unsigned int str 20 chars (18,446,744,073,709,551,615)
        cl = allocStr(vm, 21, 0);
        cl->slen = sprintf(cl->str, "%" PRIu64, GETBITS64(i));
        break;
    default:
        fprintf(stderr, "Fatal Error: ClosureType %d, not an integer type", ty);
        exit(EXIT_FAILURE);
    }
    return (VAL)cl;
}

VAL idris_castStrInt(VM* vm, VAL i) {
    char *end;
    i_int v = strtol(GETSTR(i), &end, 10);
    if (*end == '\0' || *end == '\n' || *end == '\r')
        return MKINT(v);
    else
        return MKINT(0);
}

VAL idris_castFloatStr(VM* vm, VAL i) {
    String * cl = allocStr(vm, 32, 0);
    cl->slen = snprintf(cl->str, 32, "%.16g", GETFLOAT(i));
    return (VAL)cl;
}

VAL idris_castStrFloat(VM* vm, VAL i) {
    return MKFLOAT(vm, strtod(GETSTR(i), NULL));
}

VAL idris_concat(VM* vm, VAL l, VAL r) {
    char *rs = GETSTR(r);
    char *ls = GETSTR(l);
    size_t llen = GETSTRLEN(l);
    size_t rlen = GETSTRLEN(r);

    String * cl = allocStr(vm, llen + rlen, 0);
    memcpy(cl->str, ls, llen);
    memcpy(cl->str + llen, rs, rlen);
    return (VAL)cl;
}

VAL idris_strlt(VM* vm, VAL l, VAL r) {
    char *ls = GETSTR(l);
    char *rs = GETSTR(r);

    return MKINT((i_int)(strcmp(ls, rs) < 0));
}

VAL idris_streq(VM* vm, VAL l, VAL r) {
    char *ls = GETSTR(l);
    char *rs = GETSTR(r);

    return MKINT((i_int)(strcmp(ls, rs) == 0));
}

VAL idris_strlen(VM* vm, VAL l) {
    return MKINT((i_int)(idris_utf8_strlen(GETSTR(l))));
}

VAL idris_readStr(VM* vm, FILE* h) {
    VAL ret;
    char *buffer = NULL;
    size_t n = 0;
    ssize_t len;
    len = getline(&buffer, &n, h);
    if (len <= 0) {
        ret = MKSTR(vm, "");
    } else {
        ret = MKSTR(vm, buffer);
    }
    free(buffer);
    return ret;
}

VAL idris_readChars(VM* vm, int num, FILE* h) {
    VAL ret;
    char *buffer = malloc((num+1)*sizeof(char));
    size_t len;
    len = fread(buffer, sizeof(char), (size_t)num, h);
    buffer[len] = '\0';

    if (len <= 0) {
        ret = MKSTR(vm, "");
    } else {
        ret = MKSTR(vm, buffer);
    }
    free(buffer);
    return ret;
}

void idris_crash(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

VAL idris_strHead(VM* vm, VAL str) {
    return idris_strIndex(vm, str, 0);
}

VAL MKSTROFFc(VM* vm, VAL basestr) {
    StrOffset * cl = iallocate(vm, sizeof(*cl), 1);
    SETTY(cl, CT_STROFFSET);
    cl->base = (String*)basestr;
    return (VAL)cl;
}

VAL idris_strShift(VM* vm, VAL str, int num) {
    size_t sz = sizeof(StrOffset);
    // If there's no room, just copy the string, or we'll have a problem after
    // gc moves str
    if (space(vm, sz)) {
        int offset = 0;
        StrOffset * root = (StrOffset*)str;
        StrOffset * cl = iallocate(vm, sz, 0);
        SETTY(cl, CT_STROFFSET);

        while(root!=NULL && !ISSTR(root)) { // find the root, carry on.
                              // In theory, at most one step here!
            offset += root->offset;
            root = (StrOffset*)root->base;
        }

        cl->base = (String*)root;
        cl->offset = offset+idris_utf8_findOffset(GETSTR(str), num);
        return (VAL)cl;
    } else {
        char* nstr = GETSTR(str);
        return MKSTR(vm, nstr+idris_utf8_charlen(nstr));
    }
}

VAL idris_strTail(VM* vm, VAL str) {
    return idris_strShift(vm, str, 1);
}

VAL idris_strCons(VM* vm, VAL x, VAL xs) {
    char *xstr = GETSTR(xs);
    int xval = GETINT(x);
    size_t xlen = GETSTRLEN(xs);
    String * cl;

    if (xval < 0x80) { // ASCII char
      cl = allocStr(vm, xlen + 1, 0);
        cl->str[0] = (char)(GETINT(x));
        memcpy(cl->str+1, xstr, xlen);
    } else {
        char *init = idris_utf8_fromChar(xval);
        size_t ilen = strlen(init);
        int newlen = ilen + xlen;
        cl = allocStr(vm, newlen, 0);
        memcpy(cl->str, init, ilen);
        memcpy(cl->str + ilen, xstr, xlen);
        free(init);
    }
    return (VAL)cl;
}

VAL idris_strIndex(VM* vm, VAL str, VAL i) {
    int idx = idris_utf8_index(GETSTR(str), GETINT(i));
    return MKINT((i_int)idx);
}

VAL idris_substr(VM* vm, VAL offset, VAL length, VAL str) {
    size_t offset_val = GETINT(offset);
    size_t length_val = GETINT(length);
    char* str_val = GETSTR(str);

    // If the substring is a suffix, use idris_strShift to avoid reallocating
    if (offset_val + length_val >= GETSTRLEN(str)) {
        return idris_strShift(vm, str, offset_val);
    }
    else {
        char *start = idris_utf8_advance(str_val, offset_val);
        char *end = idris_utf8_advance(start, length_val);
        size_t sz = end - start;

        if (space(vm, sz)) {
            String * newstr = allocStr(vm, sz, 0);
            memcpy(newstr->str, start, sz);
            newstr->str[sz] = '\0';
            return (VAL)newstr;
        } else {
            // Need to copy into an intermediate string before allocating,
            // because if there's no enough space then allocating will move the
            // original string!
            char* cpystr = malloc(sz);
            memcpy(cpystr, start, sz);

            String * newstr = allocStr(vm, sz, 0);
            memcpy(newstr->str, cpystr, sz);
            newstr->str[sz] = '\0';
            free(cpystr);
            return (VAL)newstr;
        }
    }
}

VAL idris_strRev(VM* vm, VAL str) {
    char *xstr = GETSTR(str);
    size_t xlen = GETSTRLEN(str);

    String * cl = allocStr(vm, xlen, 0);
    idris_utf8_rev(xstr, cl->str);
    return (VAL)cl;
}

VAL idris_newRefLock(VAL x, int outerlock) {
    Ref * cl = allocate(sizeof(*cl), outerlock);
    SETTY(cl, CT_REF);
    cl->ref = x;
    return (VAL)cl;
}

VAL idris_newRef(VAL x) {
    return idris_newRefLock(x, 0);
}

void idris_writeRef(VAL ref, VAL x) {
    Ref * r = (Ref*)ref;
    r->ref = x;
    SETTY(ref, CT_REF);
}

VAL idris_readRef(VAL ref) {
    Ref * r = (Ref*)ref;
    return r->ref;
}

VAL idris_newArray(VM* vm, int size, VAL def) {
    Array * cl;
    int i;
    cl = allocArrayF(vm, size, 0);
    for(i=0; i<size; ++i) {
        cl->array[i] = def;
    }
    return (VAL)cl;
}

void idris_arraySet(VAL arr, int index, VAL newval) {
    Array * cl = (Array*)arr;
    cl->array[index] = newval;
}

VAL idris_arrayGet(VAL arr, int index) {
    Array * cl = (Array*)arr;
    return cl->array[index];
}

VAL idris_systemInfo(VM* vm, VAL index) {
    int i = GETINT(index);
    switch(i) {
        case 0: // backend
            return MKSTR(vm, "c");
        case 1:
            return MKSTR(vm, IDRIS_TARGET_OS);
        case 2:
            return MKSTR(vm, IDRIS_TARGET_TRIPLE);
    }
    return MKSTR(vm, "");
}

int idris_errno(void) {
    return errno;
}

char* idris_showerror(int err) {
    return strerror(err);
}

Con nullary_cons[256];

void init_nullaries(void) {
    int i;
    for(i = 0; i < 256; ++i) {
        Con * cl = nullary_cons + i;
        cl->hdr.sz = sizeof(*cl);
        SETTY(cl, CT_CON);
        cl->tag = i;
    }
}

int __idris_argc;
char **__idris_argv;

int idris_numArgs(void) {
    return __idris_argc;
}

const char* idris_getArg(int i) {
    return __idris_argv[i];
}

void idris_disableBuffering(void) {
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);
}

#ifndef SEL4
int idris_usleep(int usec) {
    struct timespec t;
    t.tv_sec = usec / 1000000;
    t.tv_nsec = (usec % 1000000) * 1000;

    return nanosleep(&t, NULL);
}
#endif // SEL4

void stackOverflow(void) {
  fprintf(stderr, "Stack overflow");
  exit(-1);
}

static inline VAL copy_plain(VM* vm, VAL x, size_t sz) {
    VAL cl = iallocate(vm, sz, 1);
    memcpy(cl, x, sz);
    return cl;
}

VAL copy(VM* vm, VAL x) {
    int ar;
    VAL cl;
    if (x==NULL) {
        return x;
    }
    switch(GETTY(x)) {
    case CT_INT: return x;
    case CT_BITS32: return copy_plain(vm, x, sizeof(Bits32));
    case CT_BITS64: return copy_plain(vm, x, sizeof(Bits64));
    case CT_FLOAT: return copy_plain(vm, x, sizeof(Float));
    case CT_FWD:
        return GETPTR(x);
    case CT_CDATA:
        cl = copy_plain(vm, x, sizeof(CDataC));
        c_heap_mark_item(GETCDATA(x));
        break;
    case CT_BIGINT:
        cl = MKBIGMc(vm, GETMPZ(x));
        break;
    case CT_CON:
        ar = CARITY(x);
        if (ar == 0 && CTAG(x) < 256) {
            return x;
        }
        // FALLTHROUGH
    case CT_ARRAY:
    case CT_STRING:
    case CT_REF:
    case CT_STROFFSET:
    case CT_PTR:
    case CT_MANAGEDPTR:
    case CT_RAWDATA:
        cl = copy_plain(vm, x, x->hdr.sz);
        break;
    default:
        cl = NULL;
        assert(0);
        break;
    }
    assert(x->hdr.sz >= sizeof(Fwd));
    SETTY(x, CT_FWD);
    ((Fwd*)x)->fwd = cl;
    return cl;
}

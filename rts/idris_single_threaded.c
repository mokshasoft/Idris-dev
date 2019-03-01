#include "idris_single_threaded.h"

static VM* global_vm;

VM* get_vm_impl(void)
{
    return global_vm;
}

void idris_requireAlloc_impl(VM * vm, size_t size) {
    if (!(vm->heap.next + size < vm->heap.end)) {
        idris_gc(vm);
    }
}

void idris_doneAlloc_impl(VM * vm) {
}

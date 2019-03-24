#include "idris_single_threaded.h"
#include "idris_gc.h"

#include <assert.h>

static VM* global_vm;

void init_vm_single(VM * vm)
{
    global_vm = vm;
}

void free_vm_threaded(struct VM *vm)
{
    // nothing to free
}

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

void* iallocate_impl(VM * vm, size_t isize, int outerlock) {
    size_t size = aligned(isize);

    if (vm->heap.next + size < vm->heap.end) {
        STATS_ALLOC(vm->stats, size)
        char* ptr = vm->heap.next;
        vm->heap.next += size;
        assert(vm->heap.next <= vm->heap.end);
        ((Hdr*)ptr)->sz = isize;
        return (void*)ptr;
    } else {
        // If we're trying to allocate something bigger than the heap,
        // grow the heap here so that the new heap is big enough.
        if (size > vm->heap.size) {
            vm->heap.size += size;
        }
        idris_gc(vm);
        return iallocate_impl(vm, size, outerlock);
    }
}

#ifndef _IDRIS_SINGLE_THREADED_H
#define _IDRIS_SINGLE_THREADED_H

#include "idris_rts_types.h"

void init_vm_single(VM * vm);
void free_vm_threaded(struct VM *vm);
void idris_gc_threaded_impl(struct VM *vm);
VM* get_vm_impl(void);
void idris_requireAlloc_impl(VM * vm, size_t size);
void idris_doneAlloc_impl(VM * vm);
void* iallocate_impl(VM * vm, size_t isize, int outerlock);

#endif

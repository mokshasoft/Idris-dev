#ifndef _IDRIS_SINGLE_THREADED_H
#define _IDRIS_SINGLE_THREADED_H

#include "idris_rts_types.h"

VM* get_vm_impl(void);
void idris_requireAlloc_impl(VM * vm, size_t size);
void idris_doneAlloc_impl(VM * vm);
void* iallocate_impl(VM * vm, size_t isize, int outerlock);

#endif

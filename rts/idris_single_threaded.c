#include "idris_single_threaded.h"

static VM* global_vm;

VM* get_vm_impl(void)
{
    return global_vm;
}

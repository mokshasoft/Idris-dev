#include "idris_pthread.h"

#include <string.h>
#include <stdarg.h>

void init_vm_pthread
    ( VMPthread *pt
    , int max_threads // not implemented yet
    )
{
    pt->inbox = malloc(1024*sizeof(pt->inbox[0]));
    assert(pt->inbox);
    memset(pt->inbox, 0, 1024*sizeof(pt->inbox[0]));
    pt->inbox_end = pt->inbox + 1024;
    pt->inbox_write = pt->inbox;
    pt->inbox_nextid = 1;

    // The allocation lock must be reentrant. The lock exists to ensure that
    // no memory is allocated during the message sending process, but we also
    // check the lock in calls to allocate.
    // The problem comes when we use requireAlloc to guarantee a chunk of memory
    // first: this sets the lock, and since it is not reentrant, we get a deadlock.
    pthread_mutexattr_t rec_attr;
    pthread_mutexattr_init(&rec_attr);
    pthread_mutexattr_settype(&rec_attr, PTHREAD_MUTEX_RECURSIVE);

    pthread_mutex_init(&(pt->inbox_lock), NULL);
    pthread_mutex_init(&(pt->inbox_block), NULL);
    pthread_mutex_init(&(pt->alloc_lock), &rec_attr);
    pthread_cond_init(&(pt->inbox_waiting), NULL);

    pt->max_threads = max_threads;
    pt->processes = 0;
}

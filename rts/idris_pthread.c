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

typedef struct {
    VM* vm; // thread's VM
    VM* callvm; // calling thread's VM
    func fn;
    VAL arg;
} ThreadData;

void* runThread(void* arg) {
    ThreadData* td = (ThreadData*)arg;
    VM* vm = td->vm;
    VM* callvm = td->callvm;

    init_threaddata(vm);

    TOP(0) = td->arg;
    BASETOP(0);
    ADDTOP(1);
    td->fn(vm, NULL);
    callvm->pthread.processes--;

    free(td);

    //    Stats stats =
    terminate(vm);
    //    aggregate_stats(&(td->vm->stats), &stats);
    return NULL;
}

void* vmThread(VM* callvm, func f, VAL arg) {
    VM* vm = init_vm(callvm->stack_max - callvm->valstack, callvm->heap.size,
                     callvm->pthread.max_threads);
    vm->pthread.processes=1; // since it can send and receive messages
    pthread_t t;
    pthread_attr_t attr;
//    size_t stacksize;

    pthread_attr_init(&attr);
//    pthread_attr_getstacksize (&attr, &stacksize);
//    pthread_attr_setstacksize (&attr, stacksize*64);

    ThreadData *td = malloc(sizeof(ThreadData));
    td->vm = vm;
    td->callvm = callvm;
    td->fn = f;
    td->arg = copyTo(vm, arg);

    callvm->pthread.processes++;

    int ok = pthread_create(&t, &attr, runThread, td);
//    usleep(100);
    if (ok == 0) {
        return vm;
    } else {
        terminate(vm);
        return NULL;
    }
}

void* idris_stopThread(VM* vm) {
    close_vm(vm);
    pthread_exit(NULL);
    return NULL;
}

static VAL doCopyTo(VM* vm, VAL x);

static void copyArray(VM* vm, VAL * dst, VAL * src, size_t len) {
    size_t i;
    for(i = 0; i < len; ++i)
      dst[i] = doCopyTo(vm, src[i]);
}


// VM is assumed to be a different vm from the one x lives on

static VAL doCopyTo(VM* vm, VAL x) {
    int ar;
    VAL cl;
    if (x==NULL) {
        return x;
    }
    switch(GETTY(x)) {
    case CT_INT:
        return x;
    case CT_CDATA:
        cl = MKCDATAc(vm, GETCDATA(x));
        break;
    case CT_BIGINT:
        cl = MKBIGMc(vm, GETMPZ(x));
        break;
    case CT_CON:
        ar = CARITY(x);
        if (ar == 0 && CTAG(x) < 256) { // globally allocated
            cl = x;
        } else {
            Con * c = allocConF(vm, CTAG(x), ar, 1);
            copyArray(vm, c->args, ((Con*)x)->args, ar);
            cl = (VAL)c;
        }
        break;
    case CT_ARRAY: {
        size_t len = CELEM(x);
        Array * a = allocArrayF(vm, len, 1);
        copyArray(vm, a->array, ((Array*)x)->array, len);
        cl = (VAL)a;
    } break;
    case CT_STRING:
    case CT_FLOAT:
    case CT_PTR:
    case CT_MANAGEDPTR:
    case CT_BITS32:
    case CT_BITS64:
    case CT_RAWDATA:
        {
            cl = iallocate(vm, x->hdr.sz, 0);
            memcpy(cl, x, x->hdr.sz);
        }
        break;
    default:
        assert(0); // We're in trouble if this happens...
	cl = NULL;
    }
    return cl;
}

VAL copyTo(VM* vm, VAL x) {
    VAL ret = doCopyTo(vm, x);
    return ret;
}

// Add a message to another VM's message queue
int idris_sendMessage(VM* sender, int channel_id, VM* dest, VAL msg) {
    // FIXME: If GC kicks in in the middle of the copy, we're in trouble.
    // Probably best check there is enough room in advance. (How?)

    // Also a problem if we're allocating at the same time as the
    // destination thread (which is very likely)
    // Should the inbox be a different memory space?

    // So: we try to copy, if a collection happens, we do the copy again
    // under the assumption there's enough space this time.

    if (dest->active == 0) { return 0; } // No VM to send to

    int gcs = dest->stats.collections;
    pthread_mutex_lock(&dest->pthread.alloc_lock);
    VAL dmsg = copyTo(dest, msg);
    pthread_mutex_unlock(&dest->pthread.alloc_lock);

    if (dest->stats.collections > gcs) {
        // a collection will have invalidated the copy
        pthread_mutex_lock(&dest->pthread.alloc_lock);
        dmsg = copyTo(dest, msg); // try again now there's room...
        pthread_mutex_unlock(&dest->pthread.alloc_lock);
    }

    pthread_mutex_lock(&(dest->pthread.inbox_lock));

    if (dest->pthread.inbox_write >= dest->pthread.inbox_end) {
        // FIXME: This is obviously bad in the long run. This should
        // either block, make the inbox bigger, or return an error code,
        // or possibly make it user configurable
        fprintf(stderr, "Inbox full");
        exit(-1);
    }

    dest->pthread.inbox_write->msg = dmsg;
    if (channel_id == 0) {
        // Set lowest bit to indicate this message is initiating a channel
        channel_id = 1 + ((dest->pthread.inbox_nextid++) << 1);
    } else {
        channel_id = channel_id << 1;
    }
    dest->pthread.inbox_write->channel_id = channel_id;

    dest->pthread.inbox_write->sender = sender;
    dest->pthread.inbox_write++;

    // Wake up the other thread
    pthread_mutex_lock(&dest->pthread.inbox_block);
    pthread_cond_signal(&dest->pthread.inbox_waiting);
    pthread_mutex_unlock(&dest->pthread.inbox_block);

//    printf("Sending [signalled]...\n");

    pthread_mutex_unlock(&(dest->pthread.inbox_lock));
//    printf("Sending [unlock]...\n");
    return channel_id >> 1;
}

VM* idris_checkMessages(VM* vm) {
    return idris_checkMessagesFrom(vm, 0, NULL);
}

Msg* idris_checkInitMessages(VM* vm) {
    Msg* msg;

    for (msg = vm->pthread.inbox; msg < vm->pthread.inbox_end && msg->msg != NULL; ++msg) {
	if ((msg->channel_id & 1) == 1) { // init bit set
            return msg;
        }
    }
    return 0;
}

VM* idris_checkMessagesFrom(VM* vm, int channel_id, VM* sender) {
    Msg* msg;

    for (msg = vm->pthread.inbox; msg < vm->pthread.inbox_end && msg->msg != NULL; ++msg) {
        if (sender == NULL || msg->sender == sender) {
            if (channel_id == 0 || channel_id == msg->channel_id >> 1) {
                return msg->sender;
            }
        }
    }
    return 0;
}

VM* idris_checkMessagesTimeout(VM* vm, int delay) {
    VM* sender = idris_checkMessagesFrom(vm, 0, NULL);
    if (sender != NULL) {
        return sender;
    }

    struct timespec timeout;
    int status;

    // Wait either for a timeout or until we get a signal that a message
    // has arrived.
    pthread_mutex_lock(&vm->pthread.inbox_block);
    timeout.tv_sec = time (NULL) + delay;
    timeout.tv_nsec = 0;
    status = pthread_cond_timedwait(&vm->pthread.inbox_waiting, &vm->pthread.inbox_block,
                               &timeout);
    (void)(status); //don't emit 'unused' warning
    pthread_mutex_unlock(&vm->pthread.inbox_block);

    return idris_checkMessagesFrom(vm, 0, NULL);
}


Msg* idris_getMessageFrom(VM* vm, int channel_id, VM* sender) {
    Msg* msg;

    for (msg = vm->pthread.inbox; msg < vm->pthread.inbox_write; ++msg) {
        if (sender == NULL || msg->sender == sender) {
            if (channel_id == 0 || channel_id == msg->channel_id >> 1) {
                return msg;
            }
        }
    }
    return NULL;
}

// block until there is a message in the queue
Msg* idris_recvMessage(VM* vm) {
    return idris_recvMessageFrom(vm, 0, NULL);
}

Msg* idris_recvMessageFrom(VM* vm, int channel_id, VM* sender) {
    Msg* msg;
    Msg* ret;

    struct timespec timeout;
    int status;

    if (sender && sender->active == 0) { return NULL; } // No VM to receive from

    pthread_mutex_lock(&vm->pthread.inbox_block);
    msg = idris_getMessageFrom(vm, channel_id, sender);

    while (msg == NULL) {
//        printf("No message yet\n");
//        printf("Waiting [lock]...\n");
        timeout.tv_sec = time (NULL) + 3;
        timeout.tv_nsec = 0;
        status = pthread_cond_timedwait(&vm->pthread.inbox_waiting, &vm->pthread.inbox_block,
                               &timeout);
        (void)(status); //don't emit 'unused' warning
//        printf("Waiting [unlock]... %d\n", status);
        msg = idris_getMessageFrom(vm, channel_id, sender);
    }
    pthread_mutex_unlock(&vm->pthread.inbox_block);

    if (msg != NULL) {
        ret = malloc(sizeof(*ret));
        ret->msg = msg->msg;
        ret->sender = msg->sender;

        pthread_mutex_lock(&(vm->pthread.inbox_lock));

        // Slide everything down after the message in the inbox,
        // Move the inbox_write pointer down, and clear the value at the
        // end - O(n) but it's easier since the message from a specific
        // sender could be anywhere in the inbox

        for(;msg < vm->pthread.inbox_write; ++msg) {
            if (msg+1 != vm->pthread.inbox_end) {
                msg->sender = (msg + 1)->sender;
                msg->msg = (msg + 1)->msg;
            }
        }

        vm->pthread.inbox_write->msg = NULL;
        vm->pthread.inbox_write->sender = NULL;
        vm->pthread.inbox_write--;

        pthread_mutex_unlock(&(vm->pthread.inbox_lock));
    } else {
        fprintf(stderr, "No messages waiting");
        exit(-1);
    }
    return ret;
}

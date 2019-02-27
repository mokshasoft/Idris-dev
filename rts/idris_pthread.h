#ifndef _IDRIS_RTS_PTHREAD_H
#define _IDRIS_RTS_PTHREAD_H

#include <pthread.h>
#include "idris_rts.h"

struct Msg_t {
    struct VM* sender;
    // An identifier to say which conversation this message is part of.
    // Lowest bit is set if the id is the first message in a conversation.
    int channel_id;
    VAL msg;
};

typedef struct Msg_t Msg;

struct VMPthread {
    pthread_mutex_t inbox_lock;
    pthread_mutex_t inbox_block;
    pthread_mutex_t alloc_lock;
    pthread_cond_t inbox_waiting;

    Msg* inbox; // Block of memory for storing messages
    Msg* inbox_end; // End of block of memory
    int inbox_nextid; // Next channel id
    Msg* inbox_write; // Location of next message to write

    int processes; // Number of child processes
    int max_threads; // maximum number of threads to run in parallel
};

typedef struct VMPthread VMPthread;

void init_vm_pthread
    ( VMPthread *pt
    , int max_threads // not implemented yet
    );

#endif

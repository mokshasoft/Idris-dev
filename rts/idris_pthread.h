#ifndef _IDRIS_PTHREAD_H
#define _IDRIS_PTHREAD_H

#include <stdio.h>
#include <pthread.h>
#include <malloc.h>
#include <stdlib.h>
#include <assert.h>

#include "idris_rts_types.h"

struct VM;
typedef struct Val * VAL;
typedef void*(*func)(struct VM*, VAL*);

struct Msg_t {
    struct VM* sender;
    // An identifier to say which conversation this message is part of.
    // Lowest bit is set if the id is the first message in a conversation.
    int channel_id;
    VAL msg;
};

typedef struct Msg_t Msg;

struct VMPthread_t {
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

typedef struct VMPthread_t VMPthread;

VMPthread* alloc_vm_pthread
    ( struct VM *vm
    , int max_threads // not implemented yet
    );
void free_vm_pthread(VMPthread *pt);

struct VM* get_vm_impl(void);

void* vmThread(struct VM* callvm, func f, VAL arg);
void* idris_stopThread(struct VM* vm);

// Copy a structure to another vm's heap
VAL copyTo(struct VM* newVM, VAL x);

// Add a message to another VM's message queue
int idris_sendMessage(struct VM* sender, int channel_id, struct VM* dest, VAL msg);
// Check whether there are any messages in the queue and return PID of
// sender if so (null if not)
struct VM* idris_checkMessages(struct VM* vm);
// Check whether there are any messages which are initiating a conversation
// in the queue and return the message if so (without removing it)
Msg* idris_checkInitMessages(struct VM* vm);
// Check whether there are any messages in the queue
struct VM* idris_checkMessagesFrom(struct VM* vm, int channel_id, struct VM* sender);
// Check whether there are any messages in the queue, and wait if not
struct VM* idris_checkMessagesTimeout(struct VM* vm, int timeout);
// block until there is a message in the queue
Msg* idris_recvMessage(struct VM* vm);
// block until there is a message in the queue
Msg* idris_recvMessageFrom(struct VM* vm, int channel_id, struct VM* sender);

void idris_requireAlloc_impl(struct VM * vm, size_t size);
void idris_doneAlloc_impl(struct VM * vm);
void* iallocate_impl(struct VM * vm, size_t isize, int outerlock);

// Query/free structure used to return message data (recvMessage will malloc,
// so needs an explicit free)
VAL idris_getMsg(Msg* msg);
struct VM* idris_getSender(Msg* msg);
int idris_getChannel(Msg* msg);
void idris_freeMsg(Msg* msg);

#endif

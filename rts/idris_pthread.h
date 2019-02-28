#ifndef _IDRIS_PTHREAD_H
#define _IDRIS_PTHREAD_H

#include <pthread.h>
#include "idris_rts.h"

struct VM;

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

// Set up key for thread-local data - called once from idris_main
void init_threadkeys(void);
// Initialise thread-local data for this VM
void init_threaddata(struct VM *vm);

#endif
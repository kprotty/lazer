#ifndef _LZR_QUEUE_H
#define _LZR_QUEUE_H

#include "atomic.h"

typedef struct QueueNode {
    struct QueueNode* next;
} QueueNode;

typedef struct {
    QueueNode* tail;
    ATOMIC(QueueNode*) head;
} MpscQueue;

QueueNode* MpscQueuePop(MpscQueue* queue);
void MpscQueuePush(MpscQueue* queue, QueueNode* node);
void MpscQueueInit(MpscQueue* queue, QueueNode* stub);

typedef union {
    #if defined(LZR_64)
        __uint128_t dword;
    #elif defined(LZR_32)
        uint64_t dword;
    #endif
    struct {
        size_t counter;
        QueueNode* node;
    };
} ABAQueueNode;

typedef struct {
    ATOMIC(QueueNode*) head;
    #if defined(LZR_X86)
        ABAQueueNode tail;
    #elif defined(LZR_ARM)
        ATOMIC(QueueNode*) tail;
    #endif
} MpmcQueue;

QueueNode* MpmcQueuePop(MpmcQueue* queue);
void MpmcQueuePush(MpmcQueue* queue, QueueNode* node);
void MpmcQueueInit(MpmcQueue* queue, QueueNode* stub);

#endif // _LZR_QUEUE_H
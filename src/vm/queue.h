#ifndef _LZR_QUEUE_H
#define _LZR_QUEUE_H

#include "atomic.h"

typedef struct QueueNode {
    struct QueueNode* next;
} QueueNode;

typedef struct {
    QueueNode stub;
    QueueNode* tail;
    ATOMIC(QueueNode*) head;
} MpscQueue;

void MpscQueueInit(MpscQueue* queue);
QueueNode* MpscQueuePop(MpscQueue* queue);
void MpscQueuePush(MpscQueue* queue, QueueNode* node);

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
    QueueNode stub;
    ATOMIC(QueueNode*) head;
    #if defined(LZR_X86)
        ABAQueueNode tail;
    #elif defined(LZR_ARM)
        ATOMIC(QueueNode*) tail;
    #endif
} MpmcQueue;

void MpmcQueueInit(MpmcQueue* queue);
QueueNode* MpmcQueuePop(MpmcQueue* queue);
void MpmcQueuePush(MpmcQueue* queue, QueueNode* node);

#endif // _LZR_QUEUE_H
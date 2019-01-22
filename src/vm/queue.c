#include "queue.h"

void MpscQueueInit(MpscQueue* queue) {
    queue->stub.next = NULL;
    queue->tail = &queue->stub;
    AtomicStore(&queue->head, &queue->stub, ATOMIC_RELAXED);
}

void MpscQueuePush(MpscQueue* queue, QueueNode* node) {
    node->next = NULL;
    AtomicFence(ATOMIC_RELEASE);
    QueueNode* prev = (QueueNode*) AtomicSwap(&queue->head, node, ATOMIC_RELAXED);
    AtomicStore(&prev->next, node, ATOMIC_RELAXED);
}

QueueNode* MpscQueuePop(MpscQueue* queue) {
    QueueNode* tail = queue->tail;
    QueueNode* next = AtomicLoad(&tail->next, ATOMIC_RELAXED);

    if (next != NULL) {
        queue->tail = next;
        AtomicFence(ATOMIC_ACQUIRE);
    }

    return next;
}

void MpmcQueueInit(MpmcQueue* queue) {
    queue->stub.next = NULL;
    AtomicStore(&queue->head, &queue->stub, ATOMIC_RELAXED);

    #if defined(LZR_X86)
        queue->tail.node = &queue->stub;
        queue->tail.counter = 0;
    #elif defined(LZR_ARM)
        queue->tail = &queue->stub;
    #endif
}

void MpmcQueuePush(MpmcQueue* queue, QueueNode* node) {
    node->next = NULL;
    AtomicFence(ATOMIC_RELEASE);
    QueueNode* prev = (QueueNode*) AtomicSwap(&queue->head, node, ATOMIC_RELAXED);
    AtomicStore(&prev->next, node, ATOMIC_RELAXED);
}

QueueNode* MpmcQueuePop(MpmcQueue* q) {
    QueueNode* tail;
    QueueNode* next;
    const int ordering = ATOMIC_RELAXED;

    #if defined(LZR_X86)
        ABAQueueNode xchg;
        ABAQueueNode cmp = { .dword = q->tail.dword };
        do {
            tail = cmp.node;
            if ((next = AtomicLoad(&tail->next, ordering)) == NULL)
                return NULL;
            xchg.node = next;
            xchg.counter = cmp.counter + 1;
        } while (!AtomicCas(&q->tail.dword, &cmp.dword, xchg.dword, ordering, ordering));

    #elif defined(LZR_ARM)
        tail = AtomicLoad(&q->tail, ordering);
        do {
            if ((next = AtomicLoad(&tail->next, ordering)) == NULL)
                return NULL;
        } while(!AtomicCas(&q->tail, &tail, next, ordering, ordering));
    #endif
    
    AtomicFence(ATOMIC_ACQUIRE);
    return next;
}
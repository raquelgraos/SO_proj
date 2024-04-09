#ifndef SERVER_PATH_QUEUE_H
#define SERVER_PATH_QUEUE_H

#include <stddef.h>
#include "common/constants.h"

typedef struct PathNode {
    char path[MAX_PIPE_NAME_SIZE + 11];
    struct PathNode* next;
} PathNode;

typedef struct {
    PathNode* head;
    PathNode* tail;
    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;
} PathQueue;

extern PathQueue pathQueue;

/// Enqueues a path to the queue
/// @param path the path string
void enqueue_path(const char* path);

/// Dequeues a path from the queue
/// @return the dequeued path string
char* dequeue_path();

/// Frees the queue
void free_path_queue();
#endif  // SERVER_PATH_QUEUE_H

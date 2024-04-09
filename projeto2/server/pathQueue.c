#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


#include "common/io.h"
#include "eventlist.h"
#include "sessionFn.h"
#include "operations.h"
#include "common/constants.h"
#include "pathQueue.h"

PathQueue pathQueue = {NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};

void enqueue_path(const char* path) {
  pthread_mutex_lock(&pathQueue.mutex);

  PathNode* newNode = (PathNode*)malloc(sizeof(PathNode));
  if (newNode == NULL) {
    perror("Failed to allocate memory for path node");
    exit(EXIT_FAILURE);
  }

  strncpy(newNode->path, path, sizeof(newNode->path) - 1);
  newNode->path[sizeof(newNode->path) - 1] = '\0';

  newNode->next = NULL;

  if (pathQueue.head == NULL) {
    pathQueue.head = newNode;
    pathQueue.tail = newNode;
  } else {
    pathQueue.tail->next = newNode;
    pathQueue.tail = newNode;
  }

  pthread_mutex_unlock(&pathQueue.mutex);
}

char* dequeue_path() {
  PathNode* node = pathQueue.head;
  pathQueue.head = node->next;

  char* path = strdup(node->path);
  free(node);
  return path;
}

void free_path_queue() {
  pthread_mutex_lock(&pathQueue.mutex);

  while (pathQueue.head != NULL) {
    PathNode* node = pathQueue.head;
    pathQueue.head = node->next;
    free(node);
  }

  pthread_mutex_unlock(&pathQueue.mutex);

  pthread_mutex_destroy(&pathQueue.mutex);
  pthread_cond_destroy(&pathQueue.not_empty);
}
#ifndef THREAD_FN_H
#define THREAD_FN_H

#include <stddef.h>
#include <pthread.h>

typedef struct {
  int input_fd;
  int output_fd;
  int thread_id;
  int MAX_THREADS;
  int line_counter;
  int barried;
  int* wait_list;
  int* barrier_list;
  size_t wait_list_size;
  size_t barrier_list_size;
  unsigned int *delay_list;
  pthread_mutex_t* lock_output_writing;
  char *job_path;
} args_t;  // the arguments of each thread

/// The thread function that executes the commands
/// @param arg the arguments of the thread
void* thread_fn(void* arg);

#endif  // THREAD_FN_H

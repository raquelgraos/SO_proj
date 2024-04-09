#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "pathQueue.h"
#include "common/io.h"
#include "eventlist.h"
#include "common/constants.h"
#include "hostFn.h"
#include "sessionFn.h"
#include "operations.h"

void* host_fn(void* arg) {
  int server_pipe = *((int*)arg);
  ssize_t ret;
  char OP_CODE;

  while (1) {
    if (sigusr1_received) {
      if (ems_show_all()) {
        fprintf(stderr, "Failed to show all events\n");
      }
      sigusr1_received = 0;
    }
    ret = read(server_pipe, &OP_CODE, sizeof(char));
    if (ret == -1) {
      if (errno == EINTR) continue;
      fprintf(stderr, "Failed to read\n");
      exit(EXIT_FAILURE);
    }
    switch(OP_CODE){
      case '1':
      { 
        char req_pipe_path[MAX_PIPE_NAME_SIZE];
        ret = read(server_pipe, req_pipe_path, MAX_PIPE_NAME_SIZE);
        if (ret == -1) {
          fprintf(stderr, "Failed to read for req_pipe\n");
          exit(EXIT_FAILURE);
        }
        enqueue_path(req_pipe_path);
        
        char resp_pipe_path[MAX_PIPE_NAME_SIZE];
        ret = read(server_pipe, resp_pipe_path, MAX_PIPE_NAME_SIZE);
        if (ret == -1) {
          fprintf(stderr, "Failed to read for resp_pipe\n");
          exit(EXIT_FAILURE);
        }
        enqueue_path(resp_pipe_path);
        
        pthread_cond_signal(&pathQueue.not_empty);
        break;
      }
      default:
        fprintf(stderr, "Unknown OP_CODE: %c\n", OP_CODE);
        break;
    }
  }
  return NULL;
}
#ifndef SERVER_SESSIONFN_H
#define SERVER_SESSIONFN_H

#include <stddef.h>
#include "common/constants.h"

typedef struct {
  int req_pipe;
  int resp_pipe;
  int session_id;
  int active;
  char resp_pipe_path[MAX_PIPE_NAME_SIZE];
  char req_pipe_path[MAX_PIPE_NAME_SIZE];
} Session;

/// The session thread function that reads and writes from the client's pipes
/// @param arg the thread's arguments
void* session_fn(void* arg);

#endif  // SERVER_SESSIONFN_H

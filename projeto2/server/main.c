#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include "pathQueue.h"
#include "sessionFn.h"
#include "hostFn.h"
#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

volatile sig_atomic_t sigusr1_received = 0;

void sigusr1_handler(int signo, siginfo_t* a, void* b) {
    sigusr1_received = 1;
}

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  unlink(argv[1]);

  struct sigaction sa;
  sa.sa_sigaction = sigusr1_handler;
  sa.sa_flags = 0;

  if (sigaction(SIGUSR1, &sa, NULL) == -1) {
      perror("sigaction\n");
      return 1;
  }
  
  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  Session sessions[MAX_SESSION_COUNT];
  pthread_t tid[MAX_SESSION_COUNT];

  for (int i=0; i<MAX_SESSION_COUNT; i++){
    sessions[i].session_id= i;
    sessions[i].active = 0;
    if (pthread_create(&tid[i], NULL, session_fn, (void*)&sessions[i]) != 0) {
          fprintf(stderr, "Failed to initialize thread\n");
          return 1;
      }
  }
  
  if (mkfifo(argv[1], 0777) == -1) {
    fprintf(stderr, "Failed to create named pipe\n");
    return 1;
  }

  int server_pipe = open(argv[1], O_RDWR);
  if (server_pipe == -1) {
    fprintf(stderr, "Failed to open named pipe\n");
    return 1;
  }

  pthread_t host_tid;
  if (pthread_create(&host_tid, NULL, host_fn, (void*)&server_pipe) != 0) {
    fprintf(stderr, "Failed to initialize host thread\n");
    return 1;
  }

  if (pthread_join(host_tid, NULL) != 0) {
    fprintf(stderr, "Error joining thread\n");
    exit(EXIT_FAILURE);
  }
  
  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
      if (pthread_join(tid[i], NULL) != 0) {
          fprintf(stderr, "Error joining thread %d\n", i);
          exit(EXIT_FAILURE);
      }
  }

  free_path_queue();
  close(server_pipe);
  unlink(argv[1]);
  ems_terminate();
}
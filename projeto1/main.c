#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"
#include "threadFn.h"
#include "processFile.h"

#include <linux/limits.h>
#include <sys/stat.h>


int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  if (argc != 4) {
    fprintf(stderr, "USAGE:FIXME\n");
    return 1;
  }

  char *jobs_directory = argv[1];
  int MAX_PROC = atoi(argv[2]);
  int MAX_THREADS = atoi(argv[3]);
  
  DIR *dir = opendir(jobs_directory);
  if (!dir) {
    fprintf(stderr, "Failed to open JOBS directory\n");
    return 1;
  }

  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }


  //creates a process for each file
  struct dirent *entry;
  int status;
  int process_counter = 0;
  pid_t terminated_pid;
  while ((entry = readdir(dir)) != NULL) {
      if (check_file_extension(entry->d_name)) {
          while (process_counter >= MAX_PROC) {
              terminated_pid = wait(&status);
              if (terminated_pid == -1) {
                  fprintf(stderr, "Wait error.\n");
              } else {
                  if (WIFEXITED(status)) {
                      printf("Process %d terminated with status %d\n", terminated_pid, 
                            WEXITSTATUS(status));
                  } else if (WIFSIGNALED(status)) {
                      printf("Process %d terminated by signal %d\n", terminated_pid, 
                            WTERMSIG(status));
                  } else {
                      printf("Process %d terminated abnormally\n", terminated_pid);
                  }
                  process_counter--;
              }
          }
          pid_t pid = fork();
          if (pid == -1) {
              fprintf(stderr, "Fork did not succeed\n");
          } else if (pid == 0) {
              char job_path[PATH_MAX];
              sprintf(job_path, "%s/%s", jobs_directory, entry->d_name);
              process_file(job_path, MAX_THREADS);
              if (closedir(dir) == -1) {
                fprintf(stderr, "Failed to close directory.FIXME\n");
                ems_terminate();
                return 1;
              }
              ems_terminate();
              exit(0);
          } else {
              process_counter++;
          }
      }
  }

  while (process_counter > 0) {
    terminated_pid = wait(&status);

    if (terminated_pid == -1) {
        fprintf(stderr, "Wait error.\n");
    } else {
        if (WIFEXITED(status)) {
            printf("Process %d terminated with status %d\n", terminated_pid, 
                  WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Process %d terminated by signal %d\n", terminated_pid, 
                  WTERMSIG(status));
        } else {
            printf("Process %d terminated abnormally\n", terminated_pid);
        }

        process_counter--;
    }
  }

  if (argc > 4) {
    char *endptr;
    unsigned long int delay = strtoul(argv[1], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      ems_terminate();
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  if (closedir(dir) == -1) {
    fprintf(stderr, "Failed to close directory.FIXME\n");
    ems_terminate();
    return 1;
  }
  
  ems_terminate();
  return 0;
}
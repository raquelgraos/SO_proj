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


void* thread_fn(void* arg) {
  args_t* args = (args_t*) arg;
  int output_fd = args->output_fd;
  unsigned int event_id, delay = 0, thread_id = 0;
  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
  int input_fd = args->input_fd;
  int execute=0;
  int* result = NULL; 

  while(1) {
    if (args->barried==0){
      for (size_t i = 0; i < args->wait_list_size; i++) {
        if (args->wait_list[i] == args->line_counter) {
            delay = args->delay_list[i];
            ems_wait(delay);
        }
      }
      for (size_t i = 0; i < args->barrier_list_size; i++) {
        if (args->barrier_list[i] == args->line_counter) {
          args->barried=1;
          result = malloc(sizeof(int));
          *result = 1;
          pthread_exit(result);
        }
      } 
    } else if(args->barried==1){
      args->barried=0;
    }
    args->line_counter++;
    if (args->line_counter%args->MAX_THREADS == args->thread_id ||
        (args->line_counter%args->MAX_THREADS==0 && args->thread_id==args->MAX_THREADS)) {
      execute = 1;
    } else{
      execute = 0;
    }
    switch (get_next(input_fd)) {
      case CMD_CREATE:
        if (parse_create(input_fd, &event_id, &num_rows, &num_columns) != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (execute==1){
          if (ems_create(event_id, num_rows, num_columns)) {
            fprintf(stderr, "Failed to create event\n");
            continue;
          }
        }

        break;

      case CMD_RESERVE:
        num_coords = parse_reserve(input_fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);
        if (num_coords == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        if (execute==1){
          if (ems_reserve(event_id, num_coords, xs, ys)) {
            fprintf(stderr, "Failed to reserve seats\n");
            continue;
          }
        }
        break;

      case CMD_SHOW:
        if (parse_show(input_fd, &event_id) != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        if (execute==1){
          if (ems_show(event_id, output_fd, args->lock_output_writing)) {
            fprintf(stderr, "Failed to show event\n");
            continue;
          }
        }
        break;

      case CMD_LIST_EVENTS:
        if (execute==1){
          if (ems_list_events(output_fd, args->lock_output_writing)) {
            fprintf(stderr, "Failed to list events\n");
            continue;
          }
        }
        break;

      case CMD_WAIT:
        (parse_wait(input_fd, &delay, &thread_id));
        break;

      case CMD_INVALID:
        if (execute==1){
          fprintf(stderr, "Invalid command. See HELP for usage\n");
        }
        break;

      case CMD_HELP:
        if (execute==1){
          printf(
            "Available commands:\n"
            "  CREATE <event_id> <num_rows> <num_columns>\n"
            "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
            "  SHOW <event_id>\n"
            "  LIST\n"
            "  WAIT <delay_ms> [thread_id]\n" 
            "  BARRIER\n"                      
            "  HELP\n");
        }
        break;

      case CMD_BARRIER:
        break;
      case CMD_EMPTY:
        break;

      case EOC:
        close(input_fd);
        pthread_exit(NULL); 
    }
  }
}
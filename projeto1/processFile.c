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

#include <linux/limits.h>
#include <sys/stat.h>

void free_args(args_t* args, int MAX_THREADS){
  for (int i = 0; i < MAX_THREADS; i++) {
    if (args[i].wait_list!=NULL){
      free(args[i].wait_list);
    }
    if (args[i].delay_list!=NULL){
      free(args[i].delay_list);
    }
    if (args[i].barrier_list!=NULL){
      free(args[i].barrier_list);
    }
    if (args[i].job_path!=NULL){
      free(args[i].job_path);
    }
  }
}

int write_file(int fd, char* to_write) {
  size_t done = 0;
  size_t len = strlen(to_write);
  while (len > 0) {
    ssize_t bytes_written = write(fd, to_write + done, len);
    if (bytes_written < 0) {
      fprintf(stderr, "write error\n");   
      return -1; 
    }
    len -= (size_t)bytes_written;
    done += (size_t)bytes_written;
  }
  return 0;
}

char* remove_path_extension(char job_path_dup[]) {
  char *last_dot = strrchr(job_path_dup, '.');
  if (last_dot != NULL) {
    size_t substring_length = (size_t) (last_dot - job_path_dup);
    char *substring = (char*) malloc(sizeof(char)*(substring_length + 1));
    strncpy(substring, job_path_dup, substring_length);
    substring[substring_length] = '\0';
    return substring;
  }
  else {
    return "";
  }
}

int check_file_extension(char *name) {
  char *extension = strrchr(name, '.');
  if (strncmp(extension, ".jobs", strlen(".jobs")) == 0) {
    return 1;
  } 
  return 0;
}

int add_to_wait_list(args_t* args, int line, unsigned int delay){
    args->wait_list_size++;
    if (args->wait_list_size>1){
      args->wait_list = realloc(args->wait_list, args->wait_list_size * sizeof(int));
      args->delay_list = realloc(args->delay_list, args->wait_list_size * sizeof(unsigned int));
    }
    
    if (args->wait_list == NULL || args->delay_list == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free_args(args, args->MAX_THREADS);
        return 1;
    }
    args->wait_list[args->wait_list_size - 1] = line;
    args->delay_list[args->wait_list_size - 1] = delay;
    return 0;
}

int add_to_barrier_list(args_t* args, int line){
    args->barrier_list_size++;
    if (args->barrier_list_size>1){
      args->barrier_list = realloc(args->barrier_list, args->barrier_list_size * sizeof(int));
    }
    
    if (args->barrier_list == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free_args(args, args->MAX_THREADS);
        return 1;
    }
    args->barrier_list[args->barrier_list_size - 1] = line;
    return 0;
}

int initialize_lists(args_t* args, const char *job_path, int MAX_THREADS) {
  int input_fd_aux = open(job_path, O_RDONLY);
  int line_counter=0;
  unsigned int thread_id, delay;
  int wait_type, current_thread_id;
  unsigned int trash;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
  size_t trash2, trash3;

  if (input_fd_aux == -1) {
    fprintf(stderr, "Failed to open input file %s\n", job_path);
    close(input_fd_aux);
    return 1;
  }
  while(1) {
    line_counter++;
    current_thread_id = (line_counter % MAX_THREADS);
    if (current_thread_id==0){
      current_thread_id = MAX_THREADS;
    }
    switch (get_next(input_fd_aux)) {
      case CMD_WAIT:
        wait_type = (parse_wait(input_fd_aux, &delay, &thread_id));
        if (wait_type == 1) {
          add_to_wait_list(&args[thread_id-1], line_counter, delay); 
          continue;
        }
        else if (wait_type == 0){
          for (int i=0; i<MAX_THREADS; i++){
            add_to_wait_list(&args[i], line_counter, delay); 
          }
          continue;
        }
        break;

      case CMD_BARRIER:
        for (int i=0; i<MAX_THREADS; i++){
          add_to_barrier_list(&args[i], line_counter); 
        }
        break;


      case EOC:
        close(input_fd_aux);
        return 0;
      case CMD_CREATE: 
        if (parse_create(input_fd_aux, &trash, &trash2, &trash3) == 0) {
          continue;
        }
        break;
      case CMD_RESERVE: 
        parse_reserve(input_fd_aux, MAX_RESERVATION_SIZE, &trash, xs, ys);
        break;
      case CMD_SHOW: 
        if (parse_show(input_fd_aux, &trash) == 0) {
          continue;
        }
        break;
      case CMD_LIST_EVENTS: break;
      case CMD_HELP: break;
      case CMD_EMPTY: break;
      case CMD_INVALID: break;
    }
  }
}


void process_file(const char *job_path, int MAX_THREADS) {
  char output_path[PATH_MAX];
  char job_path_dup[PATH_MAX];
  strcpy(job_path_dup, job_path);
  char *output_name = remove_path_extension(job_path_dup);
  sprintf(output_path, "%s.out", output_name);
  free(output_name);

  int output_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (output_fd == -1) {
    fprintf(stderr, "Failed to open output file %s\n", output_path);
    return;
  }

  pthread_mutex_t shared_lock_output_writing;
  pthread_t tid[MAX_THREADS];
  args_t args[MAX_THREADS];

  for (int i = 0; i < MAX_THREADS; i++) {
    args[i].lock_output_writing = &shared_lock_output_writing;
    args[i].output_fd = output_fd;
    args[i].thread_id = i+1;
    args[i].wait_list_size = 0;
    args[i].barrier_list_size = 0;
    args[i].line_counter = 0;
    args[i].barried = 0;
    args[i].MAX_THREADS = MAX_THREADS;

    args[i].input_fd = open(job_path, O_RDONLY);
    if (args[i].input_fd == -1) {
      fprintf(stderr, "Failed to open input file %s\n", job_path);
      return;
    }

    args[i].wait_list = malloc(sizeof(int));
    args[i].barrier_list = malloc(sizeof(int));
    args[i].delay_list = malloc(sizeof(unsigned int));
    args[i].job_path = malloc(strlen(job_path) + 1);
    if (args[i].wait_list == NULL || args[i].delay_list == NULL || args[i].job_path == NULL || args[i].barrier_list == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free_args(args, MAX_THREADS);
        return;
    }

    strcpy(args[i].job_path, job_path);
  }

  if (initialize_lists(args, job_path, MAX_THREADS)){
    free_args(args,MAX_THREADS);
    close(output_fd);
    return;
  }

  if (pthread_mutex_init(&shared_lock_output_writing, NULL) != 0) {
    fprintf(stderr, "Failed to initiate lock\n");
    free_args(args,MAX_THREADS);
    close(output_fd);
    return;
  }

  for (int i = 0; i < MAX_THREADS; i++) {
      if (pthread_create(&tid[i], NULL, thread_fn, (void*)&args[i]) != 0) {
          fprintf(stderr, "Failed to initialize thread\n");
          free_args(args,MAX_THREADS);
          close(output_fd);
          return;
      }
  }
  
  int thread_results[MAX_THREADS];
  int finished=0;
  int join_result=0;
  for (int i = 0; i < MAX_THREADS; i++) {
    thread_results[i]=1;
  }
  while(1){
    for (int i = 0; i < MAX_THREADS; i++) {
      int* result;
      if (thread_results[i] == 1){
        join_result = pthread_join(tid[i], (void**)&result);
      } else if (thread_results[i] == 0){
        continue;
      }
      if (join_result != 0) {
        fprintf(stderr, "Thread join failed\n");
        free_args(args,MAX_THREADS);
        pthread_mutex_destroy(&shared_lock_output_writing);
        close(output_fd);
        return;
      }
      if (result != NULL) {
        thread_results[i] = 1;
        free(result);
      } else {
        thread_results[i] = 0;
        finished++;  
      } 
    }
    for (int i = 0; i < MAX_THREADS; i++) {
      if (thread_results[i]==1){
        if (pthread_create(&tid[i], NULL, thread_fn, (void*)&args[i]) != 0) {
          fprintf(stderr, "Failed to initialize thread\n");
          free_args(args, MAX_THREADS);
          pthread_mutex_destroy(&shared_lock_output_writing);
          close(output_fd);
          return;
        }
      }
    } if (finished==MAX_THREADS){
        free_args(args,MAX_THREADS);
        pthread_mutex_destroy(&shared_lock_output_writing);
        close(output_fd);
      return;
    }
  }
  free_args(args,MAX_THREADS);
  pthread_mutex_destroy(&shared_lock_output_writing);
  close(output_fd);
}

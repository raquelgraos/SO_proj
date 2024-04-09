#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#include "api.h"
#include "common/constants.h"
#include "common/io.h"

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(size_t num_cols, size_t row, size_t col) { return (row - 1) * num_cols + col - 1; }

Client client;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  char OP_CODE = '1';
  if (mkfifo(req_pipe_path, 0666) == -1) {
    fprintf(stderr, "Failed to create request pipe\n");
    return 1;
  }
  if (mkfifo(resp_pipe_path, 0666) == -1) {
    fprintf(stderr, "Failed to create response pipe\n");
    unlink(req_pipe_path);
  }
  int server_pipe_fd = open(server_pipe_path, O_RDWR);
  if (server_pipe_fd == -1) {
    fprintf(stderr, "Failed to open server pipe\n");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    return 1;
  }

  strcpy(client.req_pipe_path, req_pipe_path);
  strcpy(client.resp_pipe_path, resp_pipe_path);

  char message[sizeof(char) + 2*sizeof(char)*MAX_PIPE_NAME_SIZE];
  char *ptr = message;

  memcpy(ptr, &OP_CODE, sizeof(char));
  ptr += sizeof(char);
  memcpy(ptr, req_pipe_path, sizeof(char)*MAX_PIPE_NAME_SIZE);
  ptr += sizeof(char)*MAX_PIPE_NAME_SIZE;
  memcpy(ptr, resp_pipe_path, sizeof(char)*MAX_PIPE_NAME_SIZE);

  if (write(server_pipe_fd, message, sizeof(message)) == -1) {
    fprintf(stderr, "Failed to write\n");
    return 1;
  }

  client.req_pipe = open(req_pipe_path, O_WRONLY);
  if (client.req_pipe == -1) {
    fprintf(stderr, "Failed to open request pipe\n");
    return 1;
  }
  client.resp_pipe = open(resp_pipe_path, O_RDONLY);
  if (client.resp_pipe == -1) {
    fprintf(stderr, "Failed to open response pipe\n");
    return 1;
  }
  int session_id;
  ssize_t ret_read = read(client.resp_pipe, &session_id, sizeof(int));
  if (ret_read == -1) {
      fprintf(stderr, "Failed to read session_id\n");
      exit(EXIT_FAILURE);
  }
  close(server_pipe_fd);
  return 0;
}

int ems_quit(void) { 
  char OP_CODE = '2';
  ssize_t ret_write = write(client.req_pipe, &OP_CODE, sizeof(char));
  if (ret_write < 0) {
      fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
  }
  close (client.req_pipe);
  close (client.resp_pipe);
  return 1;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  char OP_CODE = '3';

  char message[sizeof(char) + sizeof(int) + sizeof(unsigned int) + 2 * sizeof(size_t)];
  char *ptr = message; 

  memcpy(ptr, &OP_CODE, sizeof(char));
  ptr += sizeof(char);
  memcpy(ptr, &client.session_id, sizeof(int));
  ptr += sizeof(int);
  memcpy(ptr, &event_id, sizeof(unsigned int));
  ptr += sizeof(unsigned int);
  memcpy(ptr, &num_rows, sizeof(size_t));
  ptr += sizeof(size_t);
  memcpy(ptr, &num_cols, sizeof(size_t));

  ssize_t ret_write = write(client.req_pipe, message, sizeof(message));
  if (ret_write == -1) {
    fprintf(stderr, "Failed to write\n");
    return 1;
  }

  int ret_value;
  ssize_t ret_read = read(client.resp_pipe, &ret_value, sizeof(int));
  if (ret_read == -1) {
    fprintf(stderr, "Failed to read\n");
    return 1;
  }
  return ret_value;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  char OP_CODE = '4';
  char message[sizeof(char) + sizeof(int) + sizeof(unsigned int) + sizeof(size_t) + 2 * num_seats * sizeof(size_t)];
  char *ptr = message;

  memcpy(ptr, &OP_CODE, sizeof(char));
  ptr += sizeof(char);
  memcpy(ptr, &client.session_id, sizeof(int));
  ptr += sizeof(int);
  memcpy(ptr, &event_id, sizeof(unsigned int));
  ptr += sizeof(unsigned int);
  memcpy(ptr, &num_seats, sizeof(size_t));
  ptr += sizeof(size_t);
  memcpy(ptr, xs, num_seats * sizeof(size_t));
  ptr += num_seats * sizeof(size_t);
  memcpy(ptr, ys, num_seats * sizeof(size_t));

  ssize_t ret_write = write(client.req_pipe, message, sizeof(message));
  if (ret_write == -1) {
    fprintf(stderr, "Failed to write\n");
    return 1;
  }
  
  int ret_value;
  ssize_t ret_read = read(client.resp_pipe, &ret_value, sizeof(int));
  if (ret_read == -1) {
    fprintf(stderr, "Failed to read\n");
    return 1;
  }
  return ret_value;
}

int ems_show(int out_fd, unsigned int event_id) {
  char OP_CODE = '5';

  char message[sizeof(char) + sizeof(int) + sizeof(unsigned int)];
  char *ptr = message;

  memcpy(ptr, &OP_CODE, sizeof(char));
  ptr += sizeof(char);
  memcpy(ptr, &client.session_id, sizeof(int));
  ptr += sizeof(int);
  memcpy(ptr, &event_id, sizeof(unsigned int));
  
  ssize_t ret_write = write(client.req_pipe, message, sizeof(message));
  if (ret_write == -1) {
    fprintf(stderr, "Failed to write\n");
    return 1;
  }

  int ret_value;
  ssize_t ret_read = read(client.resp_pipe, &ret_value, sizeof(int));
  if (ret_read == -1) {
    fprintf(stderr, "Failed to read\n");
    return 1;
  }

  if (ret_value == 0) {
    size_t num_rows, num_cols;

    ret_read = read(client.resp_pipe, &num_rows, sizeof(size_t));
    if (ret_read == -1) {
      fprintf(stderr, "Failed to read num_rows\n");
      return 1;
    }
    ret_read = read(client.resp_pipe, &num_cols, sizeof(size_t));
    if (ret_read == -1) {
      fprintf(stderr, "Failed to read num_cols\n");
      return 1;
    }

    unsigned int seats[num_rows * num_cols];
    ret_read = read(client.resp_pipe, seats, sizeof(unsigned int) * num_rows * num_cols);
    if (ret_read == -1) {
      fprintf(stderr, "Failed to read seats data\n");
      return 1;
    }

    for (size_t i = 1; i <= num_rows; i++) {
      for (size_t j = 1; j <= num_cols; j++) {
        char buffer[16];
        sprintf(buffer, "%u", seats[seat_index(num_cols, i, j)]);

        if (print_str(out_fd, buffer)) {
          fprintf(stderr, "Error writing to file descriptor\n");
          return 1;
        }

        if (j < num_cols) {
          if (print_str(out_fd, " ")) {
            fprintf(stderr, "Error writing to file descriptor\n");
            return 1;
          }
        }
      }

      if (print_str(out_fd, "\n")) {
        fprintf(stderr, "Error writing to file descriptor\n");
        return 1;
      }
    }
    return 0;
  } else {
    return 1;
  }
}

int ems_list_events(int out_fd) {
  char OP_CODE = '6';

  ssize_t ret_write = write(client.req_pipe, &OP_CODE, sizeof(char));
  ret_write = write(client.req_pipe, &client.session_id, sizeof(int));
  if (ret_write == -1) {
    fprintf(stderr, "Failed to write OP_CODE\n");
    return 1;
  }


  int ret_value;
  ssize_t ret_read = read(client.resp_pipe, &ret_value, sizeof(int));
  if (ret_read == -1) {
    fprintf(stderr, "Failed to read ret_value\n");
    return 1;
  }

  if (ret_value == 0) {
    size_t num_events;
    ret_read = read(client.resp_pipe, &num_events, sizeof(size_t));
    if (ret_read == -1) {
      fprintf(stderr, "Failed to read num_events\n");
      return 1;
    }
    if (num_events == 0) {
      if (print_str(out_fd, "No events\n")) {
        fprintf(stderr, "Error writing to file descriptor\n");
        return 1;
      }
      return 0;
    }

    unsigned int ids[num_events];
    ret_read = read(client.resp_pipe, ids, num_events * sizeof(unsigned int));
    
    int i = 0;
    while (num_events > 0) {
      if (print_str(out_fd, "Event: ")) {
        fprintf(stderr, "Error writing to file descriptor\n");
        return 1;
      }
      char id[16];
      sprintf(id, "%d\n", ids[i]);
      if (print_str(out_fd, id)) {
        fprintf(stderr, "Error writing to file descriptor\n");
        return 1;
      }
      i++;
      num_events--;
    }
    return 0;

  } else {
    return 1;
  }
}

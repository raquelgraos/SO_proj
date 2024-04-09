#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common/io.h"
#include "eventlist.h"

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_us = 0;

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event_id The ID of the event to get.
/// @param from First node to be searched.
/// @param to Last node to be searched.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event* get_event_with_delay(unsigned int event_id, struct ListNode* from, struct ListNode* to) {
  struct timespec delay = {0, state_access_delay_us * 1000};
  nanosleep(&delay, NULL);  // Should not be removed

  return get_event(event_list, event_id, from, to);
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event* event, size_t row, size_t col) { return (row - 1) * event->cols + col - 1; }

int ems_init(unsigned int delay_us) {
  if (event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  event_list = create_list();
  state_access_delay_us = delay_us;

  return event_list == NULL;
}

int ems_terminate() {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_wrlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  free_list(event_list);
  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_wrlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  if (get_event_with_delay(event_id, event_list->head, event_list->tail) != NULL) {
    fprintf(stderr, "Event already exists\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }

  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  if (pthread_mutex_init(&event->mutex, NULL) != 0) {
    pthread_rwlock_unlock(&event_list->rwl);
    free(event);
    return 1;
  }
  event->data = calloc(num_rows * num_cols, sizeof(unsigned int));

  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    pthread_rwlock_unlock(&event_list->rwl);
    free(event);
    return 1;
  }

  if (append_to_list(event_list, event) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    pthread_rwlock_unlock(&event_list->rwl);
    free(event->data);
    free(event);
    return 1;
  }

  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id, event_list->head, event_list->tail);

  pthread_rwlock_unlock(&event_list->rwl);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  if (pthread_mutex_lock(&event->mutex) != 0) {
    fprintf(stderr, "Error locking mutex\n");
    return 1;
  }

  for (size_t i = 0; i < num_seats; i++) {
    if (xs[i] <= 0 || xs[i] > event->rows || ys[i] <= 0 || ys[i] > event->cols) {
      fprintf(stderr, "Seat out of bounds\n");
      pthread_mutex_unlock(&event->mutex);
      return 1;
    }
  }

  for (size_t i = 0; i < event->rows * event->cols; i++) {
    for (size_t j = 0; j < num_seats; j++) {
      if (seat_index(event, xs[j], ys[j]) != i) {
        continue;
      }

      if (event->data[i] != 0) {
        fprintf(stderr, "Seat already reserved\n");
        pthread_mutex_unlock(&event->mutex);
        return 1;
      }

      break;
    }
  }

  unsigned int reservation_id = ++event->reservations;

  for (size_t i = 0; i < num_seats; i++) {
    event->data[seat_index(event, xs[i], ys[i])] = reservation_id;
  }

  pthread_mutex_unlock(&event->mutex);
  return 0;
}

int ems_show(int out_fd, unsigned int event_id) {
  ssize_t ret;
  int ret_value;

  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    ret_value = 1;
    ret = write(out_fd, &ret_value, sizeof(int));
    if (ret == -1) fprintf(stderr, "Failed to write\n");
    return ret_value;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    ret_value = 1;
    ret = write(out_fd, &ret_value, sizeof(int));
    if (ret == -1) fprintf(stderr, "Failed to write\n");
    return ret_value;
  }

  struct Event* event = get_event_with_delay(event_id, event_list->head, event_list->tail);

  pthread_rwlock_unlock(&event_list->rwl);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    ret_value = 1;
    ret = write(out_fd, &ret_value, sizeof(int));
    if (ret == -1) fprintf(stderr, "Failed to write\n");
    return ret_value;
  }

  if (pthread_mutex_lock(&event->mutex) != 0) {
    fprintf(stderr, "Error locking mutex\n");
    ret_value = 1;
    ret = write(out_fd, &ret_value, sizeof(int));
    if (ret == -1) fprintf(stderr, "Failed to write\n");
    return ret_value;
  }

  size_t num_rows = event->rows;
  size_t num_cols = event->cols;
  unsigned int seats[num_rows * num_cols];

  for (size_t i = 1; i <= num_rows; i++) {
    for (size_t j = 1; j <= num_cols; j++) {
      size_t seat_ix = seat_index(event, i, j);
      seats[seat_ix] = event->data[seat_ix];
    }
  }

  pthread_mutex_unlock(&event->mutex);

  ret_value = 0;
  char response[sizeof(int) + 2 * sizeof(size_t) + sizeof(unsigned int) * num_rows * num_cols];
  char *ptr = response;

  memcpy(ptr, &ret_value, sizeof(int));
  ptr += sizeof(int);
  memcpy(ptr, &num_rows, sizeof(size_t));
  ptr += sizeof(size_t);
  memcpy(ptr, &num_cols, sizeof(size_t));
  ptr += sizeof(size_t);
  memcpy(ptr, seats, sizeof(unsigned int) * num_rows * num_cols);

  ssize_t ret_write = write(out_fd, response, sizeof(response));
  if (ret_write == -1){
    fprintf(stderr, "Error writing\n");
  }
  return ret_value;
}

int ems_list_events(int out_fd) {
  int ret_value;
  ssize_t ret;

  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    ret_value = 1;
    ret = write(out_fd, &ret_value, sizeof(int));
    if (ret == -1) fprintf(stderr, "Failed to write\n");
    return ret_value;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    ret_value = 1;
    ret = write(out_fd, &ret_value, sizeof(int));
    if (ret == -1) fprintf(stderr, "Failed to write\n");
    return ret_value;
  }

  struct ListNode* to = event_list->tail;
  struct ListNode* current = event_list->head;

  size_t num_events = 0;
  while (current != NULL) {
    num_events++;
    if (current == to) {
        break;
    }
    current = current->next;
  }

  ret_value = 0;
  ret = write(out_fd, &ret_value, sizeof(int));
  if (ret == -1) fprintf(stderr, "Failed to write\n");

  ret = write(out_fd, &num_events, sizeof(size_t));
  if (ret == -1) fprintf(stderr, "Failed to write\n");

  if (num_events != 0) {
    unsigned int ids[num_events];
    int i = 0;
    current = event_list->head;
    while (current != NULL) {
        unsigned int event_id = (current->event)->id;
        ids[i] = event_id;
        if (current == to) {
            break;
        }
        i++;
        current = current->next;
    }
    ret = write(out_fd, ids, sizeof(unsigned int) * num_events);
    if (ret == -1) fprintf(stderr, "Failed to write\n");
  }
  
  pthread_rwlock_unlock(&event_list->rwl);
  return ret_value;
}

int ems_show_all() {
  if (event_list == NULL) {
      fprintf(stderr, "EMS state must be initialized\n");
      return 1;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
      fprintf(stderr, "Error locking list rwl\n");
      return 1;
  }

  struct ListNode* current = event_list->head;

  while (current != NULL) {
    unsigned int event_id = (current->event)->id;

    fprintf(stdout, "%u\n", event_id);
    struct Event* event = get_event_with_delay(event_id, event_list->head, event_list->tail);

    if (event == NULL) {
      fprintf(stderr, "Event not found\n");
      return 1;
    }

    if (pthread_mutex_lock(&event->mutex) != 0) {
      fprintf(stderr, "Error locking mutex\n");
      return 1;
    }

    size_t num_rows = event->rows;
    size_t num_cols = event->cols;

    for (size_t i = 1; i <= num_rows; i++) {
      for (size_t j = 1; j <= num_cols; j++) {
        char buffer[16];
        sprintf(buffer, "%u", event->data[seat_index(event, i, j)]);

        if (print_str(1, buffer)) {
          fprintf(stderr, "Error writing to file descriptor");
          pthread_mutex_unlock(&event->mutex);
          return 1;
        }
        if (j < event->cols) {
          if (print_str(1, " ")) {
            fprintf(stderr, "Error writing to file descriptor");
            pthread_mutex_unlock(&event->mutex);
            return 1;
          }
        }
      }      
      if (print_str(1, "\n")) {
        fprintf(stderr, "Error writing to file descriptor");
        pthread_mutex_unlock(&event->mutex);
        return 1;
      }
    }
    pthread_mutex_unlock(&event->mutex);
    if (print_str(1, "\n")) {
      fprintf(stderr, "Error writing to file descriptor");
      pthread_mutex_unlock(&event->mutex);
      return 1;
    }
    current = current->next;
  }
  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}
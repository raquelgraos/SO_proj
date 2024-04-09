#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "pathQueue.h"
#include "common/io.h"
#include "eventlist.h"
#include "common/constants.h"
#include "sessionFn.h"
#include "operations.h"


void* session_fn(void* arg) {
    Session* session = (Session*) arg;
    char OP_CODE;
    ssize_t ret;

    if (pthread_sigmask(SIG_BLOCK, &(sigset_t){SIGUSR1}, NULL) != 0) {
        fprintf(stderr, "Failed to block SIGUSR1\n");
        exit(EXIT_FAILURE);
    }
    
    while(1){
        if (session->active == 0){
            char* path;
            pthread_mutex_lock(&pathQueue.mutex);
            while (pathQueue.head == NULL) {
                pthread_cond_wait(&pathQueue.not_empty, &pathQueue.mutex);
            }
            path = dequeue_path();
            if (path != NULL) {
                strcpy(session->req_pipe_path, path);
                free(path);
            }
            while (pathQueue.head == NULL) {
                pthread_cond_wait(&pathQueue.not_empty, &pathQueue.mutex);
            }
            path = dequeue_path();
            if (path != NULL) {
                strcpy(session->resp_pipe_path, path);
                free(path);
            }
            pthread_mutex_unlock(&pathQueue.mutex);

            session->req_pipe = open(session->req_pipe_path, O_RDONLY);
            if (session->req_pipe == -1) {
                fprintf(stderr, "Failed to open request pipe\n");
                exit(EXIT_FAILURE);
            }
            session->resp_pipe = open(session->resp_pipe_path, O_WRONLY);
            if (session->resp_pipe == -1) {
                fprintf(stderr, "Failed to open response pipe\n");
                exit(EXIT_FAILURE);
            }
            ret = write(session->resp_pipe, &session->session_id, sizeof(int));
            if (ret == -1) {
                fprintf(stderr, "Failed to write for resp_pipe\n");
                exit(EXIT_FAILURE);
            }
            session->active=1;

        } else if (session->active == 1){
            ret = read(session->req_pipe, &OP_CODE, sizeof(char));
            if (ret == -1) {
                fprintf(stderr, "Failed to read OP_CODE\n");
                exit(EXIT_FAILURE);
            }
            switch(OP_CODE){
                case '2': //quit
                    close(session->req_pipe);
                    close(session->resp_pipe);
                    session->active = 0;
                    break;
                case '3': { //create
                    unsigned int event_id;
                    size_t num_rows, num_cols;
                    int session_id;
                    ret = read(session->req_pipe, &session_id, sizeof(int));
                    if (ret == -1) {
                        fprintf(stderr, "Failed to read session_id\n");
                        exit(EXIT_FAILURE);
                    }
                    ret = read(session->req_pipe, &event_id, sizeof(unsigned int));
                    if (ret == -1) {
                        fprintf(stderr, "Failed to read event_id\n");
                        exit(EXIT_FAILURE);
                    }
                    ret = read(session->req_pipe, &num_rows, sizeof(size_t));
                    if (ret == -1) {
                        fprintf(stderr, "Failed to read num_rows\n");
                        exit(EXIT_FAILURE);
                    }
                    ret = read(session->req_pipe, &num_cols, sizeof(size_t));
                    if (ret == -1) {
                        fprintf(stderr, "Failed to read num_cols\n");
                        exit(EXIT_FAILURE);
                    }

                    int res = ems_create(event_id, num_rows, num_cols);
                    ret = write(session->resp_pipe, &res, sizeof(int));
                    if (ret == -1) {
                        fprintf(stderr, "Failed to write\n");
                        exit(EXIT_FAILURE);
                    }
                    break;
                }
                case '4': { //reserve
                    unsigned int event_id;
                    int session_id;
                    size_t num_seats;
                    ret = read(session->req_pipe, &session_id, sizeof(int));
                    if (ret == -1) {
                        fprintf(stderr, "Failed to read session_id\n");
                        exit(EXIT_FAILURE);
                    }
                    ret = read(session->req_pipe, &event_id, sizeof(unsigned int));
                    if (ret == -1) {
                        fprintf(stderr, "Failed to read event_id\n");
                        exit(EXIT_FAILURE);
                    }
                    ret = read(session->req_pipe, &num_seats, sizeof(size_t));
                    if (ret == -1) {
                        fprintf(stderr, "Failed to read num_seats\n");
                        exit(EXIT_FAILURE);
                    }

                    size_t xs[num_seats];
                    ret = read(session->req_pipe, xs, num_seats * sizeof(size_t));
                    if (ret == -1) {
                        fprintf(stderr, "Failed to read xs\n");
                        exit(EXIT_FAILURE);
                    }

                    size_t ys[num_seats];
                    ret = read(session->req_pipe, ys, num_seats * sizeof(size_t));
                    if (ret == -1) {
                        fprintf(stderr, "Failed to read ys\n");
                        exit(EXIT_FAILURE);
                    }

                    int res = ems_reserve(event_id, num_seats, xs, ys);
                    ret = write(session->resp_pipe, &res, sizeof(int));
                    if (ret == -1) {
                        fprintf(stderr, "Failed to write\n");
                        exit(EXIT_FAILURE);
                    }
                    break;
                }
                case '5': { //show
                    unsigned int event_id;
                    int session_id;
                    ret = read(session->req_pipe, &session_id, sizeof(int));
                    if (ret == -1) {
                        fprintf(stderr, "Failed to read session_id\n");
                        exit(EXIT_FAILURE);
                    }
                    ret = read(session->req_pipe, &event_id, sizeof(unsigned int));
                    if (ret == -1) {
                        fprintf(stderr, "Failed to read event_id\n");
                        exit(EXIT_FAILURE);
                    }

                    ems_show(session->resp_pipe, event_id);
                    break;
                }
                case '6': { //list
                    int session_id;
                    ret = read(session->req_pipe, &session_id, sizeof(int));
                    if (ret == -1) {
                        fprintf(stderr, "Failed to read session_id\n");
                        exit(EXIT_FAILURE);
                    }
                    ems_list_events(session->resp_pipe);
                    break;
                }
                default:
                    fprintf(stderr, "Unknown OP_CODE: %c\n", OP_CODE);
                    break;
            }
        }

    }
}
#ifndef SERVER_HOSTFN_H
#define SERVER_HOSTFN_H

#include <stddef.h>
#include <signal.h>
#include <pthread.h>

#include "common/constants.h"

extern volatile sig_atomic_t sigusr1_received;

/// The host function that reads the requests to estabilish a session.
/// @param arg the arguments of the host thread
void* host_fn(void* arg);

#endif  // SERVER_HOSTFN_H
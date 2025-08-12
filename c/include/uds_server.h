#ifndef UDS_SERVER_H
#define UDS_SERVER_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <errno.h>
#include <fcntl.h>

#define SOCKET_PATH "/tmp/uds_socket"
#define BUFFER_SIZE 1024

// forward declaration
struct app_state;

struct uds_server {
    int server_fd;
    int client_fd;  // store the connected client file descriptor
    pthread_t server_thread;
    int is_running;
    int client_connected;  // flag to track if client is connected
    pthread_mutex_t server_mutex;
    struct app_state* app; // reference to app_state for video_list access
};

// server functions
int uds_server_init(struct uds_server* server);
int uds_server_start(struct uds_server* server);
void uds_server_stop(struct uds_server* server);
void uds_server_send(struct uds_server* server, char* message);
void uds_server_cleanup(struct uds_server* server);
void* uds_server_thread_func(void* arg);

#endif

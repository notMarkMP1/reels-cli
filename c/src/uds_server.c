#include "uds_server.h"
#include "video_player.h"

int uds_server_init(struct uds_server* server) {
    if (!server) {
        return -1;
    }

    memset(server, 0, sizeof(struct uds_server));
    server->server_fd = -1;
    server->client_fd = -1;
    server->is_running = 0;
    server->client_connected = 0;
    server->app = NULL;

    if (pthread_mutex_init(&server->server_mutex, NULL) != 0) {
        return -1;
    }

    return 0;
}

int uds_server_start(struct uds_server* server) {
    if (!server) {
        return -1;
    }

    pthread_mutex_lock(&server->server_mutex);
    
    if (server->is_running) {
        pthread_mutex_unlock(&server->server_mutex);
        return 0; // already running
    }

    // remove any existing socket file
    unlink(SOCKET_PATH);

    server->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server->server_fd == -1) {
        perror("socket");
        pthread_mutex_unlock(&server->server_mutex);
        return -1;
    }

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);
    
    if (bind(server->server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server->server_fd);
        server->server_fd = -1;
        pthread_mutex_unlock(&server->server_mutex);
        return -1;
    }

    if (listen(server->server_fd, 5) == -1) {
        perror("listen");
        close(server->server_fd);
        server->server_fd = -1;
        unlink(SOCKET_PATH);
        pthread_mutex_unlock(&server->server_mutex);
        return -1;
    }

    server->is_running = 1;

    if (pthread_create(&server->server_thread, NULL, uds_server_thread_func, server) != 0) {
        perror("pthread_create");
        server->is_running = 0;
        close(server->server_fd);
        server->server_fd = -1;
        unlink(SOCKET_PATH);
        pthread_mutex_unlock(&server->server_mutex);
        return -1;
    }

    pthread_mutex_unlock(&server->server_mutex);
    return 0;
}

void uds_server_stop(struct uds_server* server) {
    if (!server) {
        return;
    }

    pthread_mutex_lock(&server->server_mutex);

    if (!server->is_running) {
        pthread_mutex_unlock(&server->server_mutex);
        return;
    }

    server->is_running = 0;

    if (server->server_fd != -1) {
        close(server->server_fd);
        server->server_fd = -1;
    }

    pthread_mutex_unlock(&server->server_mutex);

    // wait for thread to finish
    pthread_join(server->server_thread, NULL);

    unlink(SOCKET_PATH);
}

void uds_server_cleanup(struct uds_server* server) {
    if (!server) {
        return;
    }

    uds_server_stop(server);
    pthread_mutex_destroy(&server->server_mutex);
}

void uds_server_send(struct uds_server* server, char* message){
    if (!server || !message) {
        return;
    }

    pthread_mutex_lock(&server->server_mutex);

    if (!server->is_running || !server->client_connected || server->client_fd == -1) { // no client dont send
        pthread_mutex_unlock(&server->server_mutex);
        return;
    }

    ssize_t bytes_sent = send(server->client_fd, message, strlen(message), 0);
    if (bytes_sent == -1) {
        perror("send to client");
        server->client_connected = 0;
        server->client_fd = -1;
    }
    pthread_mutex_unlock(&server->server_mutex);
}


void* uds_server_thread_func(void* arg) {
    struct uds_server* server = (struct uds_server*)arg;
    int client_fd;
    struct sockaddr_un client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received, bytes_sent;
    fd_set readfds;
    struct timeval timeout;
    int select_result;
    int server_fd_copy;

    while (server->is_running) {

        // get a copy of server_fd under mutex protection
        pthread_mutex_lock(&server->server_mutex);
        server_fd_copy = server->server_fd;
        pthread_mutex_unlock(&server->server_mutex);

        if (server_fd_copy == -1 || !server->is_running) {
            break;
        }

        // use select with timeout to check for incoming connections
        FD_ZERO(&readfds);
        FD_SET(server_fd_copy, &readfds);

        timeout.tv_sec = 0.5; // 0.5 seconds timeout
        timeout.tv_usec = 0;

        select_result = select(server_fd_copy + 1, &readfds, NULL, NULL, &timeout);

        if (select_result == -1) {
            if (server->is_running && errno != EBADF) {
                perror("select");
            }
            break;
        } else if (select_result == 0) {
            continue;
        }

        if (!FD_ISSET(server_fd_copy, &readfds)) {
            continue;
        }

        client_len = sizeof(client_addr);
        client_fd = accept(server_fd_copy, (struct sockaddr*)&client_addr, &client_len);

        if (client_fd == -1) {
            if (server->is_running && errno != EBADF && errno != EINVAL) {
                perror("accept");
            }
            continue;
        }

        // Store the client connection
        pthread_mutex_lock(&server->server_mutex);
        server->client_fd = client_fd;
        server->client_connected = 1;
        pthread_mutex_unlock(&server->server_mutex);

        while (server->is_running) {
            bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) {
                if (server->is_running) {
                    perror("recv");
                }
                break;
            }

            buffer[bytes_received] = '\0';

            // remove trailing newlines
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
                buffer[len - 1] = '\0';
            }

            // printf("UDS Server received from Python: %s\n", buffer);

            // add the received message to the video list
            if (server->app && server->app->video_list) {
                pthread_mutex_lock(&server->app->video_list_mutex);
                vector_push_back_unique(server->app->video_list, buffer);
                pthread_mutex_unlock(&server->app->video_list_mutex);
                // printf("Added video to list: %s\n", buffer);
            }

            // echo the message back with a confirmation
            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response), "Video added to list: %s", buffer);

            bytes_sent = send(client_fd, response, strlen(response), 0);
            if (bytes_sent == -1) {
                if (server->is_running) {
                    perror("send");
                }
                break;
            }
        }

        // Client disconnected, update state
        pthread_mutex_lock(&server->server_mutex);
        server->client_connected = 0;
        server->client_fd = -1;
        pthread_mutex_unlock(&server->server_mutex);

        close(client_fd);
    }

    // printf("UDS Server thread exiting\n");
    return NULL;
}

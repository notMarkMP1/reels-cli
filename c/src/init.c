#include "include/video_player.h"

int app_init(struct app_state* app) {
    setlocale(LC_ALL, "");

    struct notcurses_options opts = {
        .flags = NCOPTION_INHIBIT_SETLOCALE,
        .margin_t = 0,
        .margin_r = 0,
        .margin_b = 0,
        .margin_l = 0,
    };

    app->nc = notcurses_init(&opts, NULL);
    if (!app->nc) {
        fprintf(stderr, "Error initializing notcurses\n");
        return -1;
    }

    app->stdplane = notcurses_stdplane(app->nc);
    if (!app->stdplane) {
        fprintf(stderr, "Error getting standard plane\n");
        notcurses_stop(app->nc);
        return -1;
    }

    notcurses_term_dim_yx(app->nc, &app->rows, &app->cols);
    app->blitter = graphics_detect_support(app->nc);

    if (pthread_mutex_init(&app->video_list_mutex, NULL) != 0) {
        fprintf(stderr, "Error initializing video list mutex\n");
        notcurses_stop(app->nc);
        return -1;
    }

    // initialize and start the UDS server
    if (uds_server_init(&app->server) < 0) {
        fprintf(stderr, "Error initializing UDS server\n");
        pthread_mutex_destroy(&app->video_list_mutex);
        notcurses_stop(app->nc);
        return -1;
    }
    
    // set the app reference in the server
    app->server.app = app;
    
    if (uds_server_start(&app->server) < 0) {
        fprintf(stderr, "Error starting UDS server\n");
        uds_server_cleanup(&app->server);
        pthread_mutex_destroy(&app->video_list_mutex);
        notcurses_stop(app->nc);
        return -1;
    }

    return 0;
}

void app_cleanup(struct app_state* app) {
    // stop and cleanup the UDS server
    // uds_server_cleanup(&app->server); UDS SERVER CLEANUP IS BREAKING AGAIN TODO
    
    // destroy the video list mutex
    pthread_mutex_destroy(&app->video_list_mutex);
    
    free(app->video_list);

    if (app->nc) {
        notcurses_stop(app->nc);
    }
}

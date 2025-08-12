#include "video_player.h"
#include "vector.h"

int main() {
    struct app_state app = {0};
    struct video_player player = {0};

    app.video_list = malloc(sizeof(string_vector));
    vector_init(app.video_list);
    if (app_init(&app) < 0) {
        return EXIT_FAILURE;
    }
    // home page before video
    show_home_page(&app);

    notcurses_term_dim_yx(app.nc, &app.rows, &app.cols);
    notcurses_render(app.nc);
    video_plane_load(&app);

    // wait for Python client to connect
    while (1) {
        if (app.server.client_connected) {
            break;
        }
        usleep(100000); // sleep 100ms
    }
    
    // now send fetch command to connected Python client
    uds_server_send(&app.server, "fetch");
    
    while (1) {
        pthread_mutex_lock(&app.video_list_mutex);
        size_t video_list_size = app.video_list->size;
        pthread_mutex_unlock(&app.video_list_mutex);
        if (video_list_size == 0) { // dont begin the app until we have videos
            usleep(100000); // 100ms
            continue;
        }
        break;
    }

    while (!app.quit) {

        pthread_mutex_lock(&app.video_list_mutex);
        const char* current_video = vector_get(app.video_list, app.video_index);
        pthread_mutex_unlock(&app.video_list_mutex);

        if (video_load(&player, current_video) < 0) {
            app_cleanup(&app);
            return EXIT_FAILURE;
        }
        video_play(&app, &player);
        video_cleanup(&player);
    }

    vector_free(app.video_list);
    video_cleanup(&player);
    app_cleanup(&app);

    ao_shutdown();

    return EXIT_SUCCESS;
}

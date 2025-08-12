#include "include/video_player.h"

int input_check_quit(struct notcurses* nc) {
    ncinput input;
    if (notcurses_get_nblock(nc, &input) > 0) {
        if (input.id == 'q' || input.id == 'Q') {
            return 1;
        }
    }
    return 0;
}

int input_handle(struct app_state* app, struct notcurses* nc, struct video_player* player) {
    ncinput input;
    if (notcurses_get_nblock(nc, &input) > 0) {
        switch (input.id) {
            case 'q':
            case 'Q':
                app->quit = true;
                // stop the UDS server to unblock pthread_join
                // uds_server_stop(&app->server);
                return 1; // quit
            case NCKEY_SPACE: // space to toggle pause
                if (player->audio) {
                    if (player->audio->is_paused) {
                        audio_resume(player->audio);
                    } else {
                        audio_pause(player->audio);
                    }
                }
                break;
            case NCKEY_UP: // go back a video
                if (app->video_index > 0) { // dont scroll if at 0
                    app->video_index--;
                    app->video_scroll = true;
                    return 1;
                }
                break;
            case NCKEY_DOWN:
                // handle scroll input if needed
                pthread_mutex_lock(&app->video_list_mutex);
                size_t video_list_size = app->video_list->size;
                pthread_mutex_unlock(&app->video_list_mutex);
                
                if(app->video_index < (int)(video_list_size - 2)){
                    uds_server_send(&app->server, "fetch");
                }

                if (app->video_index < (int)(video_list_size - 1)) {
                    app->video_index++;
                    app->video_scroll = true;
                }
                return 1;
        }
    }
    return 0;
}

void timing_sleep_frame(void) {
    struct timespec ts = {0, FRAME_DELAY_NS};
    nanosleep(&ts, NULL);
}

double get_time_in_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

void sync_video_to_audio(struct video_player* player) {
    if (!player->audio || !player->audio->is_playing) {
        return;
    }
    
    double video_time = player->sync.video_clock;
    double audio_time = player->audio->audio_clock;
    double diff = video_time - audio_time;
    
    player->sync.audio_clock = audio_time;
    
    // if video is ahead of audio by more than frame duration, wait
    if (diff > player->frame_duration) {
        double sleep_time = diff - player->frame_duration;
        if (sleep_time > 0 && sleep_time < 0.1) { // cap at 100ms
            struct timespec ts;
            ts.tv_sec = (time_t)sleep_time;
            ts.tv_nsec = (long)((sleep_time - ts.tv_sec) * 1000000000);
            nanosleep(&ts, NULL);
        }
    }
    // if video is behind by more than 2 frame durations, skip timing adjustment
    else if (diff < -2 * player->frame_duration) {
        // video is significantly behind, skip sleep to catch up
        return;
    }
}

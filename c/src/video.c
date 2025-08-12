#include "include/video_player.h"

int video_load(struct video_player* player, const char* filename) {

    if (filename == NULL) {
        fprintf(stderr, "Error: filename is NULL\n");
        return -1;
    }

    player->filename = filename;
    player->frame_count = 0;
    player->is_playing = 0;

    memset(&player->sync, 0, sizeof(struct av_sync));
    player->fps = 30.0;
    player->frame_duration = 1.0 / player->fps;

    player->ncv = ncvisual_from_file(filename);
    if (!player->ncv) {
        fprintf(stderr, "Error opening video file '%s'\n", filename);
        return -1;
    }

    player->audio = malloc(sizeof(struct audio_player));
    if (!player->audio) {
        fprintf(stderr, "Failed to allocate memory for audio player\n");
        ncvisual_destroy(player->ncv);
        return -1;
    }

    if (audio_init(player->audio) < 0) {
        fprintf(stderr, "Failed to initialize audio player\n");
        free(player->audio);
        player->audio = NULL;
        ncvisual_destroy(player->ncv);
        return -1;
    }

    if (audio_open_url(player->audio, filename) < 0) {
        fprintf(stderr, "Warning: Failed to open audio from video file, continuing without audio\n");
        audio_cleanup(player->audio);
        free(player->audio);
        player->audio = NULL;
    }

    return 0;
}

int video_play(struct app_state* app, struct video_player* player) {

    player->is_playing = 1;

    clock_gettime(CLOCK_MONOTONIC, &player->sync.start_time);
    player->sync.frame_timer = get_time_in_seconds();
    player->sync.video_clock = 0.0;
    player->sync.audio_clock = 0.0;

    if (player->audio) {
        if (audio_play(player->audio) < 0) {
            fprintf(stderr, "Warning: Failed to start audio playback\n");
        }
    }

    double next_frame_time = get_time_in_seconds();
    struct ncplane* last_planes[120];
    for (int i = 0; i < 120; i++) {
        last_planes[i] = NULL;
    }
    struct ncplane* rendered_plane = NULL;
    while (player->is_playing) {
        if (input_handle(app, app->nc, player)) {
            app->video_scroll = false;
            break;
        }

        if (player->audio->is_paused){ // dont do anything while audio is paused
            next_frame_time = get_time_in_seconds();
            continue;
        }

        double current_time = get_time_in_seconds();

        // skip frame if we're running behind
        if (current_time < next_frame_time) {
            double sleep_time = next_frame_time - current_time;
            if (sleep_time > 0.001) { // only sleep if more than 1ms
                struct timespec ts;
                ts.tv_sec = (time_t)sleep_time;
                ts.tv_nsec = (long)((sleep_time - ts.tv_sec) * 1000000000);
                nanosleep(&ts, NULL);
            }
        }
        
        int decode_result = ncvisual_decode(player->ncv);

        if (decode_result == 1) {
            break;
        } else if (decode_result < 0) {
            fprintf(stderr, "Error decoding frame: %d\n", decode_result);
            break;
        }

        // Update video clock
        player->sync.video_clock = player->frame_count * player->frame_duration;

        rendered_plane = video_render_frame(app, player);
        if (rendered_plane == NULL) {
            fprintf(stderr, "Error rendering frame %d\n", player->frame_count);
            break;
        }else{
            // Destroy the last rendered plane to avoid memory leaks
            if (player->frame_count > 0 && player->frame_count % 120 == 0 && last_planes[player->frame_count % 120]) {
                for (int i = 0; i < 120; i++) {
                    if (last_planes[i]) {
                        ncplane_destroy(last_planes[i]);
                        last_planes[i] = NULL;
                    }
                }
            }
            last_planes[player->frame_count % 120] = rendered_plane;
        }

        // Calculate next frame time
        next_frame_time += player->frame_duration;
        
        // Sync with audio if available
        if (player->audio && player->audio->is_playing) {
            sync_video_to_audio(player);
        }
        
        player->frame_count++;
    }

    // clean up rendered planes
    for (int i = 0; i < 120; i++) {
        if (last_planes[i] != NULL) {
            ncplane_destroy(last_planes[i]);
            last_planes[i] = NULL;
        }
    }
    // Stop audio playback
    if (player->audio) {
        audio_stop(player->audio);
    }

    return 0;
}

void video_cleanup(struct video_player* player) {
    if (player->ncv) {
        ncvisual_destroy(player->ncv);
        player->ncv = NULL;
    }

    // Clean up audio player
    if (player->audio) {
        audio_cleanup(player->audio);
        free(player->audio);
        player->audio = NULL;
    }

    player->is_playing = 0;
}

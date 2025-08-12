#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include <pthread.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <ao/ao.h>
#include <unistd.h>
#include "uds_server.h"
#include "vector.h"

#define DEFAULT_FPS 30
#define FRAME_DELAY_NS 33000000 // 33ms for ~30fps
#define MAX_AUDIO_DELAY_MS 100

struct av_sync {
    double video_clock;
    double audio_clock;
    double frame_timer;
    double frame_delay;
    double video_pts;
    double audio_pts;
    struct timespec start_time;
};

struct app_state {
    struct notcurses* nc;
    struct ncplane* stdplane;
    ncblitter_e blitter;
    unsigned rows, cols;
    int video_index;
    bool video_scroll; // whether a video scroll was triggered
    bool quit; 
    struct uds_server server; // Unix domain socket server
    string_vector* video_list;
    pthread_mutex_t video_list_mutex; // mutex to protect video_list access
};

struct video_player {
    struct ncvisual* ncv;
    const char* filename;
    int frame_count;
    int is_playing;
    struct audio_player* audio;
    struct av_sync sync;
    double fps;
    double frame_duration;
};

struct audio_player {
    AVFormatContext* format_ctx;
    AVCodecContext* codec_ctx;
    AVStream* audio_stream;
    SwrContext* swr_ctx;
    ao_device* ao_device;
    ao_sample_format ao_format;
    int audio_stream_index;
    int sample_rate;
    int channels;
    int is_playing;
    int is_paused;
    pthread_t audio_thread;
    pthread_mutex_t audio_mutex;
    pthread_cond_t audio_cond;
    double audio_clock;
    double bytes_per_second;
    uint64_t total_bytes_played;
    struct timespec start_time;
};

// app initialization
int app_init(struct app_state* app);
void app_cleanup(struct app_state* app);

// home page
void show_home_page(struct app_state* app);

// video player functions
int video_load(struct video_player* player, const char* filename);
int video_play(struct app_state* app, struct video_player* player);
void video_cleanup(struct video_player* player);

// rendering functions
ncblitter_e graphics_detect_support(struct notcurses* nc);
struct ncplane* video_render_frame(struct app_state* app, struct video_player* player);
int video_plane_load(struct app_state* app);
int render_info_panel(struct app_state* app, struct video_player* player);

// input handling
int input_check_quit(struct notcurses* nc);
int input_handle(struct app_state* app, struct notcurses* nc, struct video_player* player);
void timing_sleep_frame(void);
double get_time_in_seconds(void);
void sync_video_to_audio(struct video_player* player);

// audio player functions
int audio_init(struct audio_player* player);
int audio_open_url(struct audio_player* player, const char* url);
int audio_play(struct audio_player* player);
void audio_pause(struct audio_player* player);
void audio_resume(struct audio_player* player);
void audio_stop(struct audio_player* player);
void audio_cleanup(struct audio_player* player);
void* audio_thread_func(void* arg);

#endif

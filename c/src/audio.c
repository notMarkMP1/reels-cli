#include "include/video_player.h"
#include <unistd.h>
#include <libavutil/opt.h>

#define AUDIO_BUFFER_SIZE 4096
#define PREBUFFER_FRAMES 10

int audio_init(struct audio_player* player) {
    if (!player) return -1;

    memset(player, 0, sizeof(struct audio_player));

    ao_initialize();

    if (pthread_mutex_init(&player->audio_mutex, NULL) != 0) {
        fprintf(stderr, "Failed to initialize audio mutex\n");
        return -1;
    }

    if (pthread_cond_init(&player->audio_cond, NULL) != 0) {
        fprintf(stderr, "Failed to initialize audio condition variable\n");
        pthread_mutex_destroy(&player->audio_mutex);
        return -1;
    }

    return 0;
}

int audio_open_url(struct audio_player* player, const char* url) {
    if (!player || !url) return -1;

    int ret;

    player->format_ctx = avformat_alloc_context();
    if (!player->format_ctx) {
        fprintf(stderr, "Failed to allocate format context\n");
        return -1;
    }

    ret = avformat_open_input(&player->format_ctx, url, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to open URL: %s\n", av_err2str(ret));
        return -1;
    }

    ret = avformat_find_stream_info(player->format_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to find stream info: %s\n", av_err2str(ret));
        return -1;
    }

    player->audio_stream_index = -1;
    for (unsigned int i = 0; i < player->format_ctx->nb_streams; i++) {
        if (player->format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            player->audio_stream_index = i;
            break;
        }
    }

    if (player->audio_stream_index == -1) {
        fprintf(stderr, "No audio stream found\n");
        return -1;
    }

    player->audio_stream = player->format_ctx->streams[player->audio_stream_index];

    const AVCodec* codec = avcodec_find_decoder(player->audio_stream->codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "Failed to find audio decoder\n");
        return -1;
    }

    player->codec_ctx = avcodec_alloc_context3(codec);
    if (!player->codec_ctx) {
        fprintf(stderr, "Failed to allocate codec context\n");
        return -1;
    }

    ret = avcodec_parameters_to_context(player->codec_ctx, player->audio_stream->codecpar);
    if (ret < 0) {
        fprintf(stderr, "Failed to copy codec parameters: %s\n", av_err2str(ret));
        return -1;
    }

    ret = avcodec_open2(player->codec_ctx, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to open codec: %s\n", av_err2str(ret));
        return -1;
    }

    player->sample_rate = 22050;
    player->channels = 1; // mono audio

    player->swr_ctx = swr_alloc();
    if (!player->swr_ctx) {
        fprintf(stderr, "Failed to allocate resampler context\n");
        return -1;
    }

    av_opt_set_chlayout(player->swr_ctx, "in_chlayout", &player->codec_ctx->ch_layout, 0);
    av_opt_set_int(player->swr_ctx, "in_sample_rate", player->codec_ctx->sample_rate, 0);
    av_opt_set_sample_fmt(player->swr_ctx, "in_sample_fmt", player->codec_ctx->sample_fmt, 0);

    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_MONO;
    av_opt_set_chlayout(player->swr_ctx, "out_chlayout", &out_ch_layout, 0);
    av_opt_set_int(player->swr_ctx, "out_sample_rate", player->sample_rate, 0);
    av_opt_set_sample_fmt(player->swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    ret = swr_init(player->swr_ctx);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize resampler: %s\n", av_err2str(ret));
        return -1;
    }

    int default_driver = ao_default_driver_id();

    int pulse_driver = ao_driver_id("pulse");
    int driver_to_use = (pulse_driver >= 0) ? pulse_driver : default_driver;

    player->ao_format.bits = 16;
    player->ao_format.channels = player->channels;
    player->ao_format.rate = player->sample_rate;
    player->ao_format.byte_format = AO_FMT_LITTLE;

    player->ao_device = ao_open_live(driver_to_use, &player->ao_format, NULL);
    if (!player->ao_device) {
        fprintf(stderr, "Failed to open audio device\n");
        return -1;
    }

    player->bytes_per_second = player->sample_rate * player->channels * 2;
    player->total_bytes_played = 0;
    player->audio_clock = 0.0;

    return 0;
}

void* audio_thread_func(void* arg) {
    struct audio_player* player = (struct audio_player*)arg;
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    if (!packet || !frame) {
        fprintf(stderr, "Failed to allocate packet or frame\n");
        goto cleanup;
    }

    uint8_t* audio_buffer = (uint8_t*)malloc(AUDIO_BUFFER_SIZE * player->channels);
    if (!audio_buffer) {
        fprintf(stderr, "Failed to allocate audio buffer\n");
        goto cleanup;
    }
    
    // Pre-buffer some frames to prevent initial crackling
    int prebuffer_count = 0;
    while (player->is_playing && prebuffer_count < PREBUFFER_FRAMES) {
        pthread_mutex_lock(&player->audio_mutex);
        int ret = av_read_frame(player->format_ctx, packet);
        pthread_mutex_unlock(&player->audio_mutex);

        if (ret < 0) break;
        
        if (packet->stream_index == player->audio_stream_index) {
            ret = avcodec_send_packet(player->codec_ctx, packet);
            if (ret >= 0) {
                while (avcodec_receive_frame(player->codec_ctx, frame) >= 0) {
                    int out_samples = swr_convert(player->swr_ctx, 
                                                (uint8_t**)&audio_buffer, 
                                                AUDIO_BUFFER_SIZE,
                                                (const uint8_t**)frame->data, 
                                                frame->nb_samples);

                    if (out_samples > 0) {
                        int bytes_to_play = out_samples * 2 * player->channels;
                        ao_play(player->ao_device, (char*)audio_buffer, bytes_to_play);
                        prebuffer_count++;
                    }
                    av_frame_unref(frame);
                }
            }
        }
        av_packet_unref(packet);
    }
    
    while (player->is_playing) {
        if (player->is_paused) {
            pthread_mutex_lock(&player->audio_mutex);
            while (player->is_paused && player->is_playing) {
                pthread_cond_wait(&player->audio_cond, &player->audio_mutex);
            }
            pthread_mutex_unlock(&player->audio_mutex);
            if (!player->is_playing) break;
        }

        pthread_mutex_lock(&player->audio_mutex);
        int ret = av_read_frame(player->format_ctx, packet);
        pthread_mutex_unlock(&player->audio_mutex);

        if (ret < 0) {
            break;
        }

        if (packet->stream_index != player->audio_stream_index) {
            av_packet_unref(packet);
            continue;
        }
        

        ret = avcodec_send_packet(player->codec_ctx, packet);
        av_packet_unref(packet);
        
        if (ret >= 0) {
            while (avcodec_receive_frame(player->codec_ctx, frame) >= 0) {
                int out_samples = swr_convert(player->swr_ctx, 
                                            (uint8_t**)&audio_buffer, 
                                            AUDIO_BUFFER_SIZE,
                                            (const uint8_t**)frame->data, 
                                            frame->nb_samples);

                if (out_samples > 0) {
                    int bytes_to_play = out_samples * 2 * player->channels;
                    ao_play(player->ao_device, (char*)audio_buffer, bytes_to_play);

                    player->total_bytes_played += bytes_to_play;
                    player->audio_clock = (double)player->total_bytes_played / player->bytes_per_second;
                }

                av_frame_unref(frame);
            }
        }
    }

cleanup:
    free(audio_buffer);
    av_frame_free(&frame);
    av_packet_free(&packet);
    return NULL;
}

int audio_play(struct audio_player* player) {
    if (!player) return -1;

    player->is_playing = 1;
    player->total_bytes_played = 0;
    player->audio_clock = 0.0;
    clock_gettime(CLOCK_MONOTONIC, &player->start_time);

    int ret = pthread_create(&player->audio_thread, NULL, audio_thread_func, player);
    if (ret != 0) {
        fprintf(stderr, "Failed to create audio thread: %s\n", strerror(ret));
        player->is_playing = 0;
        return -1;
    }
    
    return 0;
}

void audio_stop(struct audio_player* player) {
    if (!player) return;

    pthread_mutex_lock(&player->audio_mutex);
    player->is_playing = 0;
    player->is_paused = 0;
    pthread_cond_signal(&player->audio_cond);
    pthread_mutex_unlock(&player->audio_mutex);

    if (player->audio_thread) {
        pthread_join(player->audio_thread, NULL);
        player->audio_thread = 0;
    }
}

void audio_cleanup(struct audio_player* player) {
    if (!player) return;

    audio_stop(player);
    
    if (player->ao_device) {
        ao_close(player->ao_device);
        player->ao_device = NULL;
    }

    if (player->swr_ctx) {
        swr_free(&player->swr_ctx);
    }

    if (player->codec_ctx) {
        avcodec_free_context(&player->codec_ctx);
    }

    if (player->format_ctx) {
        avformat_close_input(&player->format_ctx);
    }

    pthread_mutex_destroy(&player->audio_mutex);
    pthread_cond_destroy(&player->audio_cond);

}

void audio_pause(struct audio_player* player) {
    if (!player) return;

    pthread_mutex_lock(&player->audio_mutex);
    player->is_paused = 1;
    pthread_mutex_unlock(&player->audio_mutex);
}

void audio_resume(struct audio_player* player) {
    if (!player) return;

    pthread_mutex_lock(&player->audio_mutex);
    player->is_paused = 0;
    pthread_cond_signal(&player->audio_cond);
    pthread_mutex_unlock(&player->audio_mutex);
}
#include "include/video_player.h"

ncblitter_e graphics_detect_support(struct notcurses* nc) {
    if (notcurses_canpixel(nc)) {
        // printf("Pixel graphics supported\n");
        return NCBLIT_PIXEL;
    } else if (notcurses_cansextant(nc)) {
        // printf("Sextant graphics supported\n");
        return NCBLIT_3x2;
    } else if (notcurses_canquadrant(nc)) {
        // printf("Quadrant graphics supported\n");
        return NCBLIT_2x2;
    } else {
        // printf("Using ASCII graphics\n");
        return NCBLIT_1x1;
    }
}

struct ncplane* video_render_frame(struct app_state* app, struct video_player* player) {

    int video_width = app->rows * 2 * 9 / 16; 
    struct ncvisual_options vopts = {
        .n = app->stdplane,
        .scaling = NCSCALE_SCALE,
        .blitter = app->blitter,
        .flags = NCVISUAL_OPTION_CHILDPLANE | NCVISUAL_OPTION_NOINTERPOLATE,
        .x = ((app->cols - video_width) / 2),
        .y = 1,
    };

    struct ncplane* rendered_plane = ncvisual_blit(app->nc, player->ncv, &vopts);
    if (!rendered_plane) {
        fprintf(stderr, "Error rendering frame %d\n", player->frame_count);
        return NULL;
    }

    if (player->frame_count % 10 == 0) {
        render_info_panel(app, player);
    }

    if (notcurses_render(app->nc)) {
        fprintf(stderr, "Error rendering screen\n");
        return NULL;
    }

    return rendered_plane;
}

int video_plane_load(struct app_state* app) {
    ncplane_erase(app->stdplane);
    notcurses_term_dim_yx(app->nc, &app->rows, &app->cols);

    // instagram gradient
    uint64_t tl = NCCHANNELS_INITIALIZER(0x1a, 0x0f, 0x1a, 0x2d, 0x1b, 0x69);  // dark purple top-left
    uint64_t tr = NCCHANNELS_INITIALIZER(0x83, 0x2a, 0x8c, 0x4c, 0x1d, 0x95);  // purple-magenta top-right
    uint64_t bl = NCCHANNELS_INITIALIZER(0x0a, 0x0a, 0x0a, 0x1a, 0x0f, 0x1a);  // very dark bottom-left
    uint64_t br = NCCHANNELS_INITIALIZER(0xfd, 0x1d, 0x1d, 0xf5, 0x6a, 0x00);  // orange-red bottom-right

    ncplane_gradient(app->stdplane, 0, 0, app->rows, app->cols, " ", 0, tl, tr, bl, br);

    uint64_t text_channel = NCCHANNELS_INITIALIZER(0xf8, 0xbb, 0xd9, 0x1a, 0x0f, 0x1a);  // light pink fg, dark purple bg
    ncplane_set_channels(app->stdplane, text_channel);

    const char* loading = "Fetching...";
    int loading_len = strlen(loading);
    int loading_x = (app->cols - loading_len) / 2;
    int loading_y = app->rows / 2;
    ncplane_putstr_yx(app->stdplane, loading_y, loading_x, loading);

    notcurses_render(app->nc);
    return 0;
}


int render_info_panel(struct app_state* app, struct video_player* player) {
    int info_panel_width = 30;

    // videos are in 1.91:1 to 9:16, do 9:16 conversion and multiply by 2 for safety
    int video_width = app->rows * 2 * 9 / 16; 
    int video_location = (app->cols - video_width) / 2;
    int video_location_width = video_location + video_width;

    float current_time = (float)player->frame_count / DEFAULT_FPS;
    int current_minutes = (int)current_time / 60;
    int current_seconds = (int)current_time % 60;

    char info_lines[10][info_panel_width];
    int line = 1;

    // video information
    snprintf(info_lines[0], info_panel_width, "VIDEO INFO");
    ncplane_putstr_yx(app->stdplane, line++, video_location_width + 1, info_lines[0]);

    line++; // empty line

    snprintf(info_lines[1], info_panel_width, "File: %.20s", strrchr(player->filename, '/') ? strrchr(player->filename, '/') + 1 : player->filename);
    ncplane_putstr_yx(app->stdplane, line++, video_location_width + 1, info_lines[1]);

    snprintf(info_lines[3], info_panel_width, "Time: %02d:%02d", current_minutes, current_seconds);
    ncplane_putstr_yx(app->stdplane, line++, video_location_width + 1, info_lines[3]);

    line++;

    // contorls
    snprintf(info_lines[5], info_panel_width, "CONTROLS");
    ncplane_putstr_yx(app->stdplane, line++, video_location_width + 1, info_lines[5]);

    line++; // empty line

    snprintf(info_lines[6], info_panel_width, "q/Q - Quit");
    ncplane_putstr_yx(app->stdplane, line++, video_location_width + 1, info_lines[6]);

    line++; // empty line

    snprintf(info_lines[7], info_panel_width, "␣ - Pause");
    ncplane_putstr_yx(app->stdplane, line++, video_location_width + 1, info_lines[7]);

    line++; // empty line

    snprintf(info_lines[8], info_panel_width, "🔼🔽 - Scroll");
    ncplane_putstr_yx(app->stdplane, line++, video_location_width + 1, info_lines[8]);

    return 0;
}

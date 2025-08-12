#include "include/video_player.h"

int check_page_size(struct app_state* app) {
    if (app->rows < 20 || app->cols < 80) {
        ncplane_erase(app->stdplane);
        notcurses_term_dim_yx(app->nc, &app->rows, &app->cols);
        int start_row = (app->rows - 1) / 2; // -2 for the instruction line
        const char* warning = "Terminal size is too small for display.";
        ncplane_putstr_yx(app->stdplane, start_row, 1, warning);

        int periods = 0;

        notcurses_render(app->nc);

        // wait for any key
        ncinput ni;
        while (!notcurses_get_nblock(app->nc, &ni)){
            char* instruction = "Press any key to exit";

            ncplane_putstr_yx(app->stdplane, start_row + 1, 1, instruction);
            ncplane_putstr_yx(app->stdplane, start_row + 1, 1 + strlen(instruction), "       "); // clear previous dots
            for (int i = 0; i < periods/5; i++) {
                ncplane_putstr_yx(app->stdplane, start_row + 1, 1 + strlen(instruction) + i, ".");
            }
            periods = (periods + 1) % 40; // cycle through 0-3
            notcurses_render(app->nc);
            nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 100000000}, NULL); // 100ms
        }
        notcurses_stop(app->nc);
    }
    return 1;
}

void show_home_page(struct app_state* app) {

    check_page_size(app);

    notcurses_term_dim_yx(app->nc, &app->rows, &app->cols);
    ncplane_erase(app->stdplane);

    // gradient
    uint64_t tl = NCCHANNELS_INITIALIZER(0x1a, 0x1a, 0x2e, 0x0a, 0x0a, 0x1a);  // dark blue-purple top-left
    uint64_t tr = NCCHANNELS_INITIALIZER(0x2a, 0x1a, 0x3e, 0x1a, 0x0a, 0x2a);  // slightly lighter top-right
    uint64_t bl = NCCHANNELS_INITIALIZER(0x0a, 0x0a, 0x1a, 0x1a, 0x1a, 0x2e);  // dark blue-black bottom-left
    uint64_t br = NCCHANNELS_INITIALIZER(0x1a, 0x0a, 0x2a, 0x2a, 0x1a, 0x3e);  // purple bottom-right

    ncplane_gradient(app->stdplane, 0, 0, app->rows, app->cols, " ", 0, tl, tr, bl, br);

    const char* ascii_art[] = {
        "██████╗ ███████╗███████╗██╗     ███████╗       ██████╗██╗     ██╗",
        "██╔══██╗██╔════╝██╔════╝██║     ██╔════╝      ██╔════╝██║     ██║",
        "██████╔╝█████╗  █████╗  ██║     ███████╗█████╗██║     ██║     ██║",
        "██╔══██╗██╔══╝  ██╔══╝  ██║     ╚════██║╚════╝██║     ██║     ██║",
        "██║  ██║███████╗███████╗███████╗███████║      ╚██████╗███████╗██║",
        "╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝╚══════╝       ╚═════╝╚══════╝╚═╝",
        "",
        ""
    };

    int art_lines = sizeof(ascii_art) / sizeof(ascii_art[0]);
    int art_width = 65; // yes magic number, idk why strlen is broken
    // centering positions
    int start_row = (app->rows - art_lines - 4) / 2; // -4 for instruction and spacing
    int start_col = (app->cols - art_width) / 2;
    
    if (start_row < 0) start_row = 0;
    if (start_col < 0) start_col = 0;
    
    for (int i = 0; i < art_lines - 2; i++) { // -2 to skip empty lines at end
        float ratio = (float)i / (art_lines - 3);
        uint8_t r = (uint8_t)(0x00 + ratio * 0xff);      // 0x00 -> 0xff
        uint8_t g = (uint8_t)(0xff - ratio * 0x80);      // 0xff -> 0x7f  
        uint8_t b = (uint8_t)(0xff - ratio * 0x40);      // 0xff -> 0xbf
        
        uint64_t channels = NCCHANNELS_INITIALIZER(r, g, b, 0x00, 0x00, 0x00);
        
        ncplane_set_channels(app->stdplane, channels);
        ncplane_putstr_yx(app->stdplane, start_row + i, start_col, ascii_art[i]);
    }
    
    uint64_t decoration_channels = NCCHANNELS_INITIALIZER(0x60, 0x60, 0x90, 0x00, 0x00, 0x00);
    ncplane_set_channels(app->stdplane, decoration_channels);
    
    // Top decoration line
    if (start_row > 0) {
        for (int x = start_col; x < start_col + art_width && x < (int)app->cols; x++) {
            ncplane_putstr_yx(app->stdplane, start_row - 1, x, "─");
        }
    }
    
    if (start_row + art_lines < (int)app->rows - 2) {
        for (int x = start_col; x < start_col + art_width && x < (int)app->cols; x++) {
            ncplane_putstr_yx(app->stdplane, start_row + art_lines - 2, x, "─");
        }
    }
    
    const char* instruction = "Press Enter to continue...";
    int instruction_row = start_row + art_lines + 1;
    int instruction_col = (app->cols - strlen(instruction)) / 2;
    if (instruction_col < 0) instruction_col = 0;

    uint64_t white_channel = NCCHANNELS_INITIALIZER(0xff,0xff,0xff,0x00,0x00,0x00);
    ncplane_set_channels(app->stdplane, white_channel);
    ncplane_putstr_yx(app->stdplane, instruction_row, instruction_col, instruction);

    notcurses_render(app->nc);

    // wait for enter key
    uint32_t id;
    ncinput ni;
    do {
        id = notcurses_get_blocking(app->nc, &ni);
        if (id == (uint32_t)-1) {
            break; // Error or EOF
        }
    } while (ni.id != NCKEY_ENTER && ni.id != '\n' && ni.id != '\r');
}

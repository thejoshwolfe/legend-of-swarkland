#include "swarkland.hpp"
#include "display.hpp"
#include "list.hpp"
#include "thing.hpp"
#include "util.hpp"
#include "geometry.hpp"
#include "decision.hpp"
#include "tas.hpp"
#include "input.hpp"

#include <stdbool.h>

#include <SDL.h>

static void process_argv(int argc, char * argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dump-script") == 0) {
            if (i + 1 >= argc)
                panic("expected argument");
            i++;
            char * filename = argv[i];
            tas_set_output_script(filename);
        } else if (strcmp(argv[i], "--script") == 0) {
            if (i + 1 >= argc)
                panic("expected argument");
            i++;
            char * filename = argv[i];
            tas_set_input_script(filename);
        } else if (strcmp(argv[i], "--tas-delay") == 0) {
            if (i + 1 >= argc)
                panic("expected argument");
            i++;
            const char * delay_str = argv[i];
            sscanf(delay_str, "%d", &tas_delay);
        } else {
            panic("unrecognized parameter");
        }
    }
}

int main(int argc, char * argv[]) {
    process_argv(argc, argv);
    init_random();

    display_init();
    swarkland_init();
    init_decisions();

    while (!request_shutdown) {
        read_input();

        run_the_game();

        render();

        SDL_Delay(17); // 60Hz or whatever
    }

    display_finish();
    return 0;
}

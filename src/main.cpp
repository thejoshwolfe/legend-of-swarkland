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

static void help(const char * error_message, const char * argument) {
    if (error_message != nullptr) {
        fprintf(stderr, "%s", error_message);
        if (argument != nullptr) {
            fprintf(stderr, " %s", argument);
        }
        fprintf(stderr, "\n");
    }
    fprintf(stderr,
        "usage: [options...]\n"
        "\n"
        "  --dump-script path.swarklandtas\n"
        "    record game to the specified file.\n"
        "\n"
        "  --script path.swarklandtas\n"
        "    playback game from the specified file.\n"
        "\n"
        "  --tas-delay N\n"
        "    wait N frames (at 60Hz) between each step of the replay.\n"
        "\n"
        "");
    exit(1);
}
static void process_argv(int argc, char * argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            help(nullptr, nullptr);
        } else if (strcmp(argv[i], "--dump-script") == 0) {
            if (i + 1 >= argc)
                help("expected argument after", argv[i]);
            i++;
            char * filename = argv[i];
            tas_set_output_script(filename);
        } else if (strcmp(argv[i], "--script") == 0) {
            if (i + 1 >= argc)
                help("expected argument after", argv[i]);
            i++;
            char * filename = argv[i];
            tas_set_input_script(filename);
        } else if (strcmp(argv[i], "--tas-delay") == 0) {
            if (i + 1 >= argc)
                help("expected argument", argv[i]);
            i++;
            const char * delay_str = argv[i];
            sscanf(delay_str, "%d", &tas_delay);
        } else {
            help("unrecognized parameter:", argv[i]);
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

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
        //                                                                              |80
        "usage: [options] [save.swarkland]\n"
        "\n"
        "  save.swarkland\n"
        "    the default file name for your save game.\n"
        "    if you die, this file will be deleted.\n"
        "\n"
        "  --new\n"
        "    start a new game, and clobber save.swarkland (if it exists).\n"
        "\n"
        "  --replay\n"
        "    read and replay from save.swarkland, but do not modify it.\n"
        "\n"
        "  --resume\n"
        "    (the default) replay from save.swarkland (if it exists)\n"
        "    and continue writing to it. this default was chosen to\n"
        "    support double-click style running the game for casual players.\n"
        "\n"
        "  --no-save\n"
        "    don't touch save.swarkland.\n"
        "\n"
        "  --replay-delay N\n"
        "    wait N frames (at 60Hz) between each step of the replay.\n"
        "\n"
        "  --record-test\n"
        "    record a test scripts. implies --new.\n"
        "\n"
        "  --headless\n"
        "    run the input script and exit without creating a window. implies --replay.\n"
        "\n"
        //                                                                              |80
        "");
    exit(1);
}
static void process_argv(int argc, char * argv[]) {
    const char * save_name = nullptr;
    int tas_delay = 0;
    TasScriptMode mode = TasScriptMode_READ_WRITE;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                help(nullptr, nullptr);
            } else if (strcmp(argv[i], "--new") == 0) {
                mode = TasScriptMode_WRITE;
            } else if (strcmp(argv[i], "--replay") == 0) {
                mode = TasScriptMode_READ;
            } else if (strcmp(argv[i], "--resume") == 0) {
                mode = TasScriptMode_READ_WRITE;
            } else if (strcmp(argv[i], "--no-save") == 0) {
                mode = TasScriptMode_IGNORE;
            } else if (strcmp(argv[i], "--replay-delay") == 0) {
                if (i + 1 >= argc)
                    help("expected argument", argv[i]);
                i++;
                const char * delay_str = argv[i];
                sscanf(delay_str, "%d", &tas_delay);
            } else if (strcmp(argv[i], "--record-test") == 0) {
                mode = TasScriptMode_WRITE;
                test_mode = true;
            } else if (strcmp(argv[i], "--headless") == 0) {
                mode = TasScriptMode_READ;
                headless_mode = true;
            } else {
                help("unrecognized argument:", argv[i]);
            }
        } else {
            if (save_name != nullptr) {
                help("too many args", nullptr);
            }
            save_name = argv[i];
        }
    }

    if (save_name == nullptr)
        save_name = "save.swarkland";

    set_tas_delay(tas_delay);
    set_tas_script(mode, save_name);
}

int main(int argc, char * argv[]) {
    process_argv(argc, argv);
    init_random();

    if (!headless_mode)
        init_display();
    swarkland_init();
    init_decisions();

    while (!request_shutdown) {
        if (!headless_mode)
            read_input();

        run_the_game();

        if (!headless_mode) {
            render();
            SDL_Delay(17); // 60Hz or whatever
        } else {
            // that's all folks
            break;
        }
    }

    if (!headless_mode)
        display_finish();
    return 0;
}

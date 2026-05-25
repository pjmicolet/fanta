#include "tui.hpp"
#include "cpu.hpp"
#include <string>
#include <vector>
#include <iostream>

int main(int argc, char* argv[]) {
    CPU cpu{};
    
    std::string dump_file = "";
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--dump" && i + 1 < argc) {
            dump_file = argv[i + 1];
            i++;
        }
    }

    if (!dump_file.empty()) {
        // Headless mode for AI verification
        TUI tui(cpu);
        // We skip init_ncurses() for pure headless, but we need term_w/h set
        tui.dump_state(dump_file);
        return 0;
    }

    TUI tui(cpu);
    tui.init_ncurses();
    tui.run();

    return 0;
}

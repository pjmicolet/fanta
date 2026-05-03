#include "tui.hpp"
#include "cpu.hpp"

int main() {
    CPU cpu{};
    
    // Start with a clean slate - no ROM or manual stores loaded.

    TUI tui(cpu);
    tui.init_ncurses();
    tui.run();

    return 0;
}

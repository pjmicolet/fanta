#include "tui.hpp"
#include "line.hpp"
#include "cpu.hpp"

int main() {
    CPU cpu{};
    cpu.load_rom(generate_line()); 
    
    // Some initial state for testing
    cpu.store(200, 10);
    cpu.store(204, 20);
    cpu.store(208, 15);
    cpu.store(212, 15);
    cpu.store(216, 0xFFFFFFFF);

    TUI tui(cpu);
    tui.init_ncurses();
    tui.run();

    return 0;
}

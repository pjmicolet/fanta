#include "line.hpp"
#include <SDL.h>
#include "cpu.hpp"
#include "string_assembler.hpp"
#include <SDL_events.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>

constexpr int FPS = 60;
constexpr int TIMING = 1000 / FPS;
constexpr int VM_WIDTH = 320;
constexpr int VM_HEIGHT = 240;
constexpr int WINDOW_SCALE = 3;

int main(int argc, char* argv[]) {

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Fanta Console",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        VM_WIDTH * WINDOW_SCALE, VM_HEIGHT * WINDOW_SCALE,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window) {
        std::cerr << "Window Creation Error: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, 
        -1, 
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer) {
        std::cerr << "Renderer Creation Error: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        VM_WIDTH, VM_HEIGHT
    );

    if (!texture) {
        std::cerr << "Texture Creation Error: " << SDL_GetError() << "\n";
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;
    CPU cpu{};
  
    if (argc > 1) {
        std::string filename = argv[1];
        std::ifstream in(filename);
        if (!in) {
            std::cerr << "Error: Could not open " << filename << "\n";
            return 1;
        }
      
        std::vector<std::string> lines;
        std::string line;
        std::string full_code;
        while (std::getline(in, line)) {
            lines.push_back(line);
            full_code += line + "\n";
        }
      
        Assembler assem;
        assem.scan_for_labels(full_code);
        for (size_t i = 0; i < lines.size(); ++i) {
            uint32_t instr = assem.assemble(lines[i], i * 4);
            cpu.store(i * 4, (instr != (uint32_t)-1) ? instr : (0x14 << 26)); // Fallback to NOP if assembly fails
        }
        std::cout << "Loaded " << filename << " (" << lines.size() << " lines)\n";
    } else {
        cpu.load_rom(generate_line()); 
        cpu.store(200, 10);
        cpu.store(204, 20);
        cpu.store(208, 15);
        cpu.store(212, 15);
        cpu.store(216, 0xFFFFFFFF);
        std::cout << "Running default line demo\n";
    }

    while(running) {
        while(SDL_PollEvent(&event)) {
            if(event.type == SDL_QUIT) {
                running = false;
            }
        }

        // Run a larger batch of cycles per frame for smooth real-time execution
        for(int i = 0; i < 50000; i++) {
            if (!cpu.halted)
                cpu.run_cycle();
        }

        SDL_UpdateTexture(texture, nullptr, cpu.get_vram(), VM_WIDTH * sizeof(uint32_t));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

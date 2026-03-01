#include "line.hpp"
#include <SDL.h>
#include "cpu.hpp"
#include <SDL_events.h>
#include <iostream>
#include <vector>

constexpr int FPS = 60;
constexpr int TIMING = 1000 / FPS;
constexpr int VM_WIDTH = 320;
constexpr int VM_HEIGHT = 240;
constexpr int WINDOW_SCALE = 3;

int main() {

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      std::cerr << "SDL_Init Error: " << SDL_GetError() << "\n";
      return 1;
  }

  // 2. Create the Window
  // SDL_WINDOW_ALLOW_HIGHDPI is crucial for crisp text/rendering on macOS Retina screens
  SDL_Window* window = SDL_CreateWindow(
      "Fantasy Console",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      VM_WIDTH * WINDOW_SCALE, VM_HEIGHT * WINDOW_SCALE,
      SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
  );

  if (!window) {
      std::cerr << "Window Creation Error: " << SDL_GetError() << "\n";
      SDL_Quit();
      return 1;
  }

  // 3. Create the Renderer (The GPU Context)
  // SDL_RENDERER_ACCELERATED: Use Metal/OpenGL/DirectX
  // SDL_RENDERER_PRESENTVSYNC: Cap framerate to monitor refresh (prevents 1000FPS loops)
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

  // 4. Create the Texture (The GPU-side buffer)
  // SDL_TEXTUREACCESS_STREAMING: Tells GPU we will update this texture frequently (every frame)
  SDL_Texture* texture = SDL_CreateTexture(
      renderer,
      SDL_PIXELFORMAT_ARGB8888, // Matches standard uint32_t (0xAARRGGBB) layout
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
  cpu.load_rom(generate_line()); 
  std::vector<uint32_t> vram(320*240, 0xFF000000);
  uint64_t count = 0; 
  uint64_t threshold = 1000;
  uint64_t lower_threshold = 100;
  bool up = true;
  cpu.store(200, 10);
  cpu.store(204, 20);
  cpu.store(208, 15);
  cpu.store(212, 15);
  cpu.store(216, 0xFFFFFFFF);
  while(running) {
    while(SDL_PollEvent(&event)) {
      if(event.type == SDL_QUIT) {
        running = false;
      }
    }

    for(int i = 0; i < 1000; i++) {
      cpu.run_cycle();
    }
    //  for(int j = 0; j < 240; j++) {
    //  for(int i = 0; i < 320; i++) {
    //    if(cpu.load(600+(j*320+(i*4))) != 0) {
    //      std::cout << "x";
    //    } else {
    //      std::cout << ".";
    //    }
    //  }
    //  std::cout << "\n";
    //}
    //std::cout << "\n\n\n\n\n\n\n";
    // D. Render Phase
    // 1. Lock/Update the Texture with the VM's VRAM
    //    (pitch = row width in bytes = 320 * 4 bytes)
    SDL_UpdateTexture(texture, nullptr, cpu.get_vram(), VM_WIDTH * sizeof(uint32_t));

    // 2. Clear Screen (Black)
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // 3. Copy Texture to Window (automatically scales up)
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);

    // 4. Swap Buffers (Show the frame)
    SDL_RenderPresent(renderer);
  }

  // --- Cleanup ---
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

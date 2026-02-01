#include <SDL.h>
#include "cpu.hpp"
#include <SDL_events.h>
#include <iostream>
#include <array>

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
  while(running) {
    while(SDL_PollEvent(&event)) {
      if(event.type == SDL_QUIT) {
        running = false;
      }
    }
  }

  return 0;
}

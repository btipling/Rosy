// Rosy.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Rosy.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include "rhi/RHI.h"
#include "config/Config.h"
#include "utils/utils.h"

int main(int argc, char* argv[])
{
    const int WIDTH = 640;
    const int HEIGHT = 480;
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;

    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Rosy", WIDTH, HEIGHT, SDL_WINDOW_VULKAN);
    renderer = SDL_CreateRenderer(window, NULL);

    rosy_config::Config cfg = {};
    rosy_config::debug();

    Rhi* rhi = new Rhi{ cfg };

    VkResult result = rhi->init();
    if (result != VK_SUCCESS) {
        rosy_utils::DebugPrintA("rhi init failed %d\n", result);
        delete rhi;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    rhi->debug();

    bool should_run = true;
    while (should_run) {
        SDL_Event windowEvent;
        while (SDL_PollEvent(&windowEvent)) {
            if (windowEvent.type == SDL_EVENT_QUIT) {
                should_run = false;
                break;
            }
            else {
                // do nothing
            }
        }
    }
    delete rhi;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

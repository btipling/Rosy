// Rosy.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Rosy.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <thread>
#include <vector>
#define VOLK_IMPLEMENTATION
#include "volk/volk.h"
#define IMGUI_IMPL_VULKAN_USE_VOLK
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
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

    rosy_config::Config cfg = {};
    rosy_config::debug();

    Rhi* rhi = new Rhi{ cfg };

    VkResult result = rhi->init(window);
    if (result != VK_SUCCESS) {
        rosy_utils::DebugPrintA("rhi init failed %d\n", result);
        delete rhi;
        {
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
        }
        return 1;
    }
    rhi->debug();

    bool should_run = true;
    bool should_render = true;
    while (should_run) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                should_run = false;
                break;
            }
            if (event.type == SDL_EVENT_WINDOW_MINIMIZED) {
                should_render = false;
            }
            if (event.type == SDL_EVENT_WINDOW_RESTORED) {
                should_render = true;
            }
            if (!should_render) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            {
                ImGui_ImplVulkan_NewFrame();
                ImGui_ImplSDL3_NewFrame();
                ImGui::NewFrame();
                ImGui::ShowDemoWindow();
                ImGui::Render();
            }

            result = rhi->drawFrame();
            if (result != VK_SUCCESS) {
                rosy_utils::DebugPrintA("rhi draw failed %d\n", result);
                should_run = false;
            }
        }
    }
    delete rhi;
    {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
    return 0;
}

#pragma once

#include "resource.h"
#include <Windows.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "volk/volk.h"
#include "vma/vk_mem_alloc.h"
#include <ktxvulkan.h>
#include <optional>
#include <expected>
#include <vector>
#include <span>
#include <fstream>
#include <array>
#include <deque>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "config/Config.h"
#include "utils/utils.h"

namespace rh
{
	struct ctx;
}

struct state_debouncer
{
    SDL_Time last_toggled = 0;
    double delay = 0.1f;
    bool state = true;
    bool toggle()
    {
        SDL_Time current_time = 0;
        if (!SDL_GetCurrentTime(&current_time))
        {
            rosy_utils::debug_print_a("failed to get current ticks!\n");
            return state;
        }
        if (current_time - delay > last_toggled)
        {
            state = !state;
            last_toggled = current_time;
            rosy_utils::debug_print_a("updated state to %d!\n", state);
        }
        return state;
    };
};

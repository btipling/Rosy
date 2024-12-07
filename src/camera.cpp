#include "camera.h"
#include "rhi/rhi_frame.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/transform.hpp"

glm::mat4 camera::get_view_matrix() const
{
    const glm::mat4 camera_translation = translate(glm::mat4(1.f), position);
    const glm::mat4 camera_rotation = get_rotation_matrix();
    return inverse(camera_translation * camera_rotation);
}

glm::mat4 camera::get_rotation_matrix() const
{
    const glm::quat pitch_rotation = angleAxis(pitch, glm::vec3{ 1.f, 0.f, 0.f });
    const glm::quat yaw_rotation = angleAxis(yaw, glm::vec3{ 0.f, -1.f, 0.f });

    return toMat4(yaw_rotation) * toMat4(pitch_rotation);
}

void camera::process_sdl_event(const rh::ctx& ctx)
{
    if (ctx.sdl_event == nullptr) return;
    const bool* current_key_states = SDL_GetKeyboardState(nullptr);

    //if (const SDL_Scancode w_sc = SDL_GetScancodeFromKey(SDLK_W, nullptr); current_key_states[w_sc]) velocity.z = 1.f;
    //if (const SDL_Scancode w_sc = SDL_GetScancodeFromKey(SDLK_S, nullptr); current_key_states[w_sc]) velocity.z = -1.f;
    //if (const SDL_Scancode w_sc = SDL_GetScancodeFromKey(SDLK_A, nullptr); current_key_states[w_sc]) velocity.x = 1.f;
    //if (const SDL_Scancode w_sc = SDL_GetScancodeFromKey(SDLK_D, nullptr); current_key_states[w_sc]) velocity.x = -1.f;
    //if (const SDL_Scancode w_sc = SDL_GetScancodeFromKey(SDLK_SPACE, nullptr); current_key_states[w_sc]) velocity.y = 1.f;
    //if (const SDL_Scancode w_sc = SDL_GetScancodeFromKey(SDLK_Z, nullptr); current_key_states[w_sc]) velocity.y = -1.f;
    const SDL_Event e = *ctx.sdl_event;
    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_W) { velocity.z = 1.f; }
        if (e.key.key == SDLK_S) { velocity.z = -1.f; }
        if (e.key.key == SDLK_A) { velocity.x = -1.f; }
        if (e.key.key == SDLK_D) { velocity.x = 1.f; }
        if (e.key.key == SDLK_SPACE) { velocity.y = 1.f; }
        if (e.key.key == SDLK_Z) { velocity.y = -1.f; }


    }

    if (e.type == SDL_EVENT_KEY_UP) {
        if (e.key.key == SDLK_W) { velocity.z = 0; }
        if (e.key.key == SDLK_S) { velocity.z = 0; }
        if (e.key.key == SDLK_A) { velocity.x = 0; }
        if (e.key.key == SDLK_D) { velocity.x = 0; }
        if (e.key.key == SDLK_SPACE) { velocity.y = 0; }
        if (e.key.key == SDLK_Z) { velocity.y = 0; }
    }
    if (!ctx.mouse_enabled) return;
    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        yaw -= e.motion.xrel / 500.f;
        pitch += e.motion.yrel / 500.f;
    }
}

void camera::update(const rh::ctx& ctx)
{
    const glm::mat4 camera_rotation = get_rotation_matrix();
    position += glm::vec3(camera_rotation * glm::vec4(velocity * 0.05f, 0.f));
}

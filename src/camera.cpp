#include "camera.h"
#include "rhi/rhi_frame.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/transform.hpp"

glm::mat4 camera::get_view_matrix() const
{
    const glm::mat4 camera_translation = glm::translate(glm::mat4(1.f), position);
    const glm::mat4 camera_rotation = get_rotation_matrix();
    return glm::inverse(camera_translation * camera_rotation);
}

glm::mat4 camera::get_rotation_matrix() const
{
    const glm::quat pitch_rotation = glm::angleAxis(pitch, glm::vec3{ 1.f, 0.f, 0.f });
    const glm::quat yaw_rotation = glm::angleAxis(yaw, glm::vec3{ 0.f, -1.f, 0.f });

    return glm::toMat4(yaw_rotation) * glm::toMat4(pitch_rotation);
}

void camera::process_sdl_event(const rh::ctx& ctx)
{
    const SDL_Event e = *ctx.sdl_event;
    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_W) { velocity.z = -1; }
        if (e.key.key == SDLK_S) { velocity.z = 1; }
        if (e.key.key == SDLK_A) { velocity.x = -1; }
        if (e.key.key == SDLK_D) { velocity.x = 1; }
    }

    if (e.type == SDL_EVENT_KEY_UP) {
        if (e.key.key == SDLK_W) { velocity.z = 0; }
        if (e.key.key == SDLK_S) { velocity.z = 0; }
        if (e.key.key == SDLK_A) { velocity.x = 0; }
        if (e.key.key == SDLK_D) { velocity.x = 0; }
    }

    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        yaw += e.motion.xrel / 200.f;
        pitch -= e.motion.yrel / 200.f;
    }
}

void camera::update()
{
    const glm::mat4 camera_rotation = get_rotation_matrix();
    position += glm::vec3(camera_rotation * glm::vec4(velocity * 0.5f, 0.f));
}

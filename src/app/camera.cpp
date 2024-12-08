#include "camera.h"
#include "math.h"
#include "rhi/rhi_frame.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "../math.h"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/transform.hpp"

camera::camera( const glm::vec3 position) : position{position}
{
    movements_.reserve(6);
}

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

void camera::integrate_all()
{
    constexpr double base_velocity = 0.00025;
    if (velocity.x != 0)
    {
        integrate(movement::direction::horizontal, velocity.x * base_velocity);
    }
    if (velocity.y != 0)
    {
        integrate(movement::direction::vertical, velocity.y * base_velocity);
    }
    if (velocity.z != 0)
    {
        integrate(movement::direction::depth, velocity.z * base_velocity);
    }
}

void camera::integrate(const movement::direction direction, const double velocity)
{
    SDL_Time  ticks = 0;
    if (!SDL_GetCurrentTime(&ticks)) return;
    bool found = false;
    for (size_t i{ 0 }; i < movements_.size(); i++)
    {
        if (movement mv = movements_[i]; mv.dir == direction)
        {
            mv.step.current_state.velocity = velocity;
            movements_[i] = mv;
            found = true;
        }
    }
    if (!found) {
        movements_.push_back({
            .step = {
                .time = static_cast<double>(ticks),
                .accumulator = 0.f,
                .previous_state = {},
                .current_state = {
                    .position = 0,
                    .velocity = velocity,
                },
            },
            .start = static_cast<double>(ticks),
            .dir = direction,
            });
    }
}

void camera::process_sdl_event(const rh::ctx& ctx)
{
    if (ctx.sdl_event == nullptr) return;
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
    integrate_all();
    int to_remove{ -1 };
    const glm::mat4 camera_rotation = get_rotation_matrix();
    glm::vec4 vel = { 0.f, 0.f, 0.f, 0.f };
    SDL_Time tick = 0;
    if (!SDL_GetCurrentTime(&tick)) return;
    const auto dt = static_cast<double>(tick);
    bool updated = false;
    for (size_t i{ 0 }; i < movements_.size(); i++)
    {
	    movement mv = movements_[i];
        const auto new_step = physics::time_step(mv.step, dt);
        movements_[i].step = new_step;
        if (rosy_math::is_equal(new_step.current_state.velocity, 0.0))
        {
            to_remove = i;
        }
	    switch (mv.dir)
	    {
		case movement::horizontal:
            updated = true;
            vel[0] = mv.step.current_state.position;
			break;
		case movement::vertical:
            updated = true;
            vel[1] = mv.step.current_state.position;
			break;
        case movement::direction::depth:
            updated = true;
            vel[2] = mv.step.current_state.position;
            break;
	    }
    }
    if (to_remove > -1) movements_.erase(movements_.begin() + to_remove);
    if (!updated) return;
    //rosy_utils::debug_print_a("moving velocity is: (%f, %f, %f)\n",
    //    vel[0], vel[1], vel[2]);
    position += glm::vec3(camera_rotation * vel);
}

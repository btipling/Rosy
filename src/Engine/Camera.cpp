#include "pch.h"
#include "Camera.h"
#include <vector>
#define GLM_ENABLE_EXPERIMENTAL
#include <format>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/transform.hpp"

using namespace rosy;

namespace
{
    struct movement
    {
        enum direction : uint8_t
        {
            depth,
            horizontal,
            vertical,
        };

        double position = 0;
        double velocity = 0;
        direction dir;
    };

    std::array<float, 16> mat4_to_array(glm::mat4 m)
    {
        std::array<float, 16> rv{};
        const auto pos_r = glm::value_ptr(m);
        for (uint64_t i{0}; i < 16; i++) rv[i] = pos_r[i];
        return rv;
    }
}

struct synthetic_camera
{
    std::shared_ptr<rosy_logger::log> l{nullptr};
    config cfg{};

    glm::vec3 velocity{0.f};
    glm::vec3 position;
    float pitch{0.f};
    float yaw{0.f};
    std::vector<movement> movements;
    bool go_fast{false};

    result init()
    {
        movements.reserve(6);
        return result::ok;
    }

    void deinit()
    {
        movements.clear();
    }

    // ReSharper disable once CppMemberFunctionMayBeStatic
    glm::mat4 get_projection([[maybe_unused]] double g, const double s, const double n, const double f,
                             const double fov)
    {
        // Assuming VK NDC in camera for now.
        constexpr auto ndc = glm::mat4(
            glm::vec4(1.f, 0.f, 0.f, 0.f),
            glm::vec4(0.f, -1.f, 0.f, 0.f),
            glm::vec4(0.f, 0.f, -1.f, 0.f),
            glm::vec4(0.f, 0.f, 0.f, 1.f)
        );

        const double h = 1.0 / tan(fov * 0.5);
        const double w = h / s;
        const double a = -n / (f - n);
        const double b = (n * f) / (f - n);

        return glm::mat4(
            glm::vec4(w, 0, 0, 0),
            glm::vec4(0, h, 0, 0),
            glm::vec4(0, 0, a, 1.f),
            glm::vec4(0, 0, b, 0)) * ndc;
    }

    [[nodiscard]] glm::mat4 get_view_matrix() const
    {
        const glm::mat4 camera_translation = translate(glm::mat4(1.f), position);
        const glm::mat4 camera_rotation = get_rotation_matrix();
        return inverse(camera_translation * camera_rotation);
    }

    [[nodiscard]] glm::mat4 get_rotation_matrix() const
    {
        const glm::quat pitch_rotation = angleAxis(pitch, glm::vec3{-1.f, 0.f, 0.f});
        const glm::quat yaw_rotation = angleAxis(yaw, glm::vec3{0.f, 1.f, 0.f});

        return toMat4(yaw_rotation) * toMat4(pitch_rotation);
    }

    void add_movement(const movement::direction direction, const double v)
    {
        bool found = false;
        for (size_t i{0}; i < movements.size(); i++)
        {
            if (movement mv = movements[i]; mv.dir == direction)
            {
                mv.velocity = v;
                movements[i] = mv;
                found = true;
            }
        }
        if (!found)
        {
            movements.push_back({
                .position = 0,
                .velocity = v,
                .dir = direction,
            });
        }
    }

    result update(const double dt)
    {
        double base_velocity = 5.0;
        base_velocity = go_fast ? base_velocity * 2.0 : base_velocity;
        if (velocity.x != 0.f)
        {
            add_movement(movement::direction::horizontal, std::abs(velocity.x) * base_velocity);
        }
        if (velocity.y != 0.f)
        {
            add_movement(movement::direction::vertical, std::abs(velocity.y) * base_velocity);
        }
        if (velocity.z != 0.f)
        {
            add_movement(movement::direction::depth, std::abs(velocity.z) * base_velocity);
        }
        glm::vec4 vel{0.f};
        for (size_t i{0}; i < movements.size(); i++)
        {
            auto& [mv_position, mv_velocity, dir] = movements[i];
            mv_position += mv_velocity * dt;
            switch (dir)
            {
            case movement::horizontal:
                vel[0] = static_cast<float>(mv_position) * velocity.x;
                break;
            case movement::vertical:
                vel[1] = static_cast<float>(mv_position) * velocity.y;
                break;
            case movement::direction::depth:
                vel[2] = static_cast<float>(mv_position) * velocity.z;
                break;
            }
        }
        movements.clear();
        position += glm::vec3(get_rotation_matrix() * vel);
        return result::ok;
    }

    void move(const camera::direction dir, const float speed)
    {
        switch (dir)
        {
        case camera::direction::x_neg:
            velocity.x = -1.f * speed;
            break;
        case camera::direction::x_pos:
            velocity.x = 1.f * speed;
            break;
        case camera::direction::y_neg:
            velocity.y = -1.f * speed;
            break;
        case camera::direction::y_pos:
            velocity.y = 1.f * speed;
            break;
        case camera::direction::z_neg:
            velocity.z = -1.f * speed;
            break;
        case camera::direction::z_pos:
            velocity.z = 1.f * speed;
            break;
        }
    }

    void set_position(const std::array<float, 3> new_position, [[maybe_unused]] const float fov)
    {
        float y = position[1];
        const float max_y_view_space = 2.f * fov;
        position = glm::vec3(new_position[0], new_position[1], new_position[2]);
        const auto max_view_in_world_space = glm::vec3(
            glm::inverse(get_view_matrix()) * glm::vec4(0.f, -max_y_view_space, 0.f, 0.f));
        position += glm::vec3(get_rotation_matrix() * glm::vec4{0.f, 0.f, -max_view_in_world_space.y / 7.5f, 0.f});
        position[1] = y;
    }


    void set_yaw_around_position(const float new_yaw, [[maybe_unused]] const std::array<float, 3> new_position,
                                 const float fov)
    {
        float y = position[1];
        position = {new_position[0], new_position[1], new_position[2]};
        yaw = new_yaw;
        const float max_y_view_space = 2.f * fov;
        const auto max_view_in_world_space = glm::vec3(
            glm::inverse(get_view_matrix()) * glm::vec4(0.f, -max_y_view_space, 0.f, 0.f));
        position += glm::vec3(get_rotation_matrix() * glm::vec4{0.f, 0.f, -max_view_in_world_space.y / 7.5f, 0.f});
        position[1] = y;
    }
};

result camera::init(const std::shared_ptr<rosy_logger::log>& new_log, const config cfg)
{
    if (new_log == nullptr)
    {
        return result::invalid_argument;
    }
    {
        l = new_log;
        l->info("Camera initializing");
    }
    {
        // Init synthetic camera
        sc = new(std::nothrow) synthetic_camera;
        if (sc == nullptr)
        {
            l->error("synthetic_camera allocation failed");
            return result::allocation_failure;
        }
        sc->l = new_log;
        sc->cfg = cfg;
        sc->position = glm::vec3(starting_x, starting_y, starting_z);
        sc->pitch = starting_pitch;
        sc->yaw = starting_yaw;
        if (const auto res = sc->init(); res != result::ok)
        {
            l->error("graphics_device initialization failed");
            return result::graphics_init_failure;
        }
        v = mat4_to_array(sc->get_view_matrix());
    }
    return result::ok;
}

void camera::deinit()
{
    l->info("Graphics deinit start");

    if (sc)
    {
        sc->deinit();
        delete sc;
        sc = nullptr;
    }
}

// ReSharper disable once CppMemberFunctionMayBeStatic
result camera::update(const uint32_t new_viewport_width, const uint32_t new_viewport_height, const double dt)
{
    if (const auto res = sc->update(dt); res != result::ok)
    {
        return res;
    }
    viewport_width = static_cast<float>(new_viewport_width);
    viewport_height = static_cast<float>(new_viewport_height);
    s = viewport_width / viewport_height;
    const auto proj = sc->get_projection(g, s, n, f, glm::radians(fov));
    const auto view = sc->get_view_matrix();
    p = mat4_to_array(proj);
    v = mat4_to_array(view);
    vp = mat4_to_array(proj * view);
    r = mat4_to_array(sc->get_rotation_matrix());
    yaw = sc->yaw;
    pitch = sc->pitch;
    const auto pos_r = glm::value_ptr(sc->position);
    for (uint64_t i{0}; i < 3; i += 1) position[i] = pos_r[i];

    return result::ok;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void camera::move(const direction dir, const float speed) const
{
    sc->move(dir, speed);
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void camera::pitch_in_dir(const float vel)
{
    sc->pitch += vel;
    pitch = sc->pitch;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void camera::yaw_in_dir(const float vel)
{
    sc->yaw -= vel;
    yaw = sc->yaw;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void camera::set_yaw_around_position(const float new_yaw, const std::array<float, 3> new_position)
{
    sc->set_yaw_around_position(new_yaw, new_position, static_cast<float>(fov));
    yaw = sc->yaw;
    const auto pos_r = glm::value_ptr(sc->position);
    for (uint64_t i{0}; i < 3; i += 1) position[i] = pos_r[i];
}

void camera::set_game_cam_position(const std::array<float, 3> new_position)
{
    sc->set_position(new_position, static_cast<float>(fov));
    const auto pos_r = glm::value_ptr(sc->position);
    for (uint64_t i{0}; i < 3; i += 1) position[i] = pos_r[i];
}

void camera::reposition(const float new_yaw, const float new_pitch, const std::array<float, 4> new_position)
{
    sc->pitch = new_pitch;
    pitch = sc->pitch;
    sc->yaw = new_yaw;
    pitch = sc->pitch;
    sc->position = glm::vec3(new_position[0], new_position[1], new_position[2]);
    for (size_t i{0}; i < 3; i += 1) position[i] = sc->position[static_cast<int>(i)];
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void camera::go_fast() const
{
    sc->go_fast = true;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void camera::go_slow() const
{
    sc->go_fast = false;
}

#pragma once
#include "Types.h"
#include "Logger/Logger.h"
#include <array>

struct synthetic_camera;

namespace rosy
{
    struct camera
    {
        enum class direction: uint8_t { x_neg, x_pos, y_neg, y_pos, z_neg, z_pos };

        std::shared_ptr<rosy_logger::log> l{nullptr};

        synthetic_camera* sc{nullptr};
        float starting_x{0.f};
        float starting_y{0.f};
        float starting_z{0.f};
        float starting_pitch{0.f};
        float starting_yaw{0.f};
        float viewport_width{0.f};
        float viewport_height{0.f};

        double g{0.5f}; // projection plane distance
        double s{1.f}; // aspect ratio
        double n{0.1f}; // near plane
        double f{1000.f}; // far plane
        double fov{70.f}; // field of view
        std::array<float, 16> p; // projection
        std::array<float, 16> v; // view
        std::array<float, 16> vp; // view projection
        std::array<float, 16> r; // camera rotation
        std::array<float, 4> position; // camera position
        float pitch{0.f};
        float yaw{0.f};

        result init(const std::shared_ptr<rosy_logger::log>& new_log, config cfg);
        void deinit();
        result update(uint32_t new_viewport_width, uint32_t new_viewport_height, const double dt);
        void move(direction dir, float speed) const;
        void pitch_in_dir(float vel);
        void yaw_in_dir(float vel);
        void set_yaw_around_position(const float new_yaw, const std::array<float, 3> new_position);
        void set_game_cam_position(std::array<float, 3> new_position);
        void reposition(const float new_yaw, const float new_pitch, const std::array<float, 4> new_position);
        void go_fast() const;
        void go_slow() const;
    };
}

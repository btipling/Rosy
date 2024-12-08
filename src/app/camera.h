#pragma once
#include "../Rosy.h"
#include "rhi/rhi_frame.h"
#include "./physics/physics.h"



struct movement
{
    enum direction
    {
        depth,
        horizontal,
        vertical,
    };
    physics::time_ctx step;
    double start;
    direction dir;
};

class camera
{
public:
    explicit camera(glm::vec3 position);
    glm::vec3 velocity{0.f};
    glm::vec3 position;
    float pitch{ 0.f };
    float yaw{ 0.f };

    glm::mat4 get_view_matrix() const;
    glm::mat4 get_rotation_matrix() const;

    void process_sdl_event(const rh::ctx& ctx);

    void update(const rh::ctx& ctx);
private:
    std::vector<movement> movements_;
    void integrate_all();
    void integrate(movement::direction direction, double velocity);
};


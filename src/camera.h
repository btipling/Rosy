#pragma once
#include "Rosy.h"

class camera
{
public:
    glm::vec3 velocity;
    glm::vec3 position;
    float pitch{ 0.f };
    float yaw{ 0.f };

    glm::mat4 get_view_matrix() const;
    glm::mat4 get_rotation_matrix() const;

    void process_sdl_event(const rh::ctx& ctx);

    void update();
};


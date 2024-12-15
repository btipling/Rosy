#pragma once
#include "../rhi/rhi_frame.h"

class scene
{
public:
    virtual rh::result build(const rh::ctx& ctx) = 0;
    virtual rh::result update(const rh::ctx& ctx) = 0;
    virtual rh::result depth(rh::ctx ctx) = 0;
    virtual rh::result draw(rh::ctx ctx) = 0;
    virtual rh::result draw_ui(const rh::ctx& ctx) = 0;
    virtual rh::result deinit(rh::ctx& ctx) = 0;

    virtual ~scene() = default;
    scene() = default;
    scene(const scene& other) = default;
    scene(scene&& other) noexcept = default;
    scene& operator=(const scene& other) = default;
    scene& operator=(scene&& other) noexcept = default;
};

class scene_selector
{
public:
    int selected_scene = 0;
    bool updated = true;
    void draw_ui();
};
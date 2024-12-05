#pragma once

namespace physics
{
    struct state
    {
        double position;
        double velocity;
        state operator*(const double value) const;
        state operator+(const double value) const;
        state operator*(const state other) const;
        state operator+(const state other) const;
    };

    struct derivative
    {
        double dx_velocity;      // dx_velocity/dt = velocity
        double dv_acceleration;  // dv_acceleration/dt = acceleration
        derivative operator*(const double value) const;
        derivative operator+(const double value) const;
        derivative operator*(const derivative other) const;
        derivative operator+(const derivative other) const;
    };

    constexpr double delta_time = 0.01f;

    struct time_ctx
    {
        double time = 0.0f;
        double accumulator = 0.0;
        state previous_state;
        state current_state;

    };

    derivative evaluate(const state& initial, double t, double dt, const derivative& d);

    double acceleration(const state& state, double t);

    void integrate(state& state, double t, double frame_time);

    time_ctx time_step(const time_ctx& ctx, double frame_time);


}
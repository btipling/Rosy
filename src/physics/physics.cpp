#include "physics.h"

namespace physics
{
    derivative derivative::operator*(const double  value) const
    {
        derivative rv{};
        rv.dx_velocity = dx_velocity * value;
        rv.dv_acceleration = dv_acceleration * value;
        return rv;
    }

    derivative derivative::operator+(const double value) const
    {
        derivative rv{};
        rv.dx_velocity = dx_velocity + value;
        rv.dv_acceleration = dv_acceleration + value;
        return rv;
    }

    derivative derivative::operator*(const derivative other) const
    {
        derivative rv{};
        rv.dx_velocity = dx_velocity * other.dx_velocity;
        rv.dv_acceleration = dv_acceleration * other.dv_acceleration;
        return rv;
    }

    derivative derivative::operator+(const derivative other) const
    {
        derivative rv{};
        rv.dx_velocity = dx_velocity + other.dx_velocity;
        rv.dv_acceleration = dv_acceleration + other.dv_acceleration;
        return rv;
    }

    state state::operator*(const double value) const
    {
        state rv{};
        rv.position = position * value;
        rv.velocity = velocity * value;
        return rv;
    }


    state state::operator+(const double value) const
    {
        state rv{};
        rv.position = position + value;
        rv.velocity = velocity + value;
        return rv;
    }

    state state::operator*(const state other) const
    {
        state rv{};
        rv.position = position * other.position;
        rv.velocity = velocity * other.velocity;
        return rv;
    }

    state state::operator+(const state other) const
    {
        state rv{};
        rv.position = position + other.position;
        rv.velocity = velocity + other.velocity;
        return rv;
    }

    derivative evaluate(const state& initial, const double t, const double dt, const derivative& d) {
        state state{};
        state.position = initial.position + d.dx_velocity * dt;
        state.velocity = initial.velocity + d.dv_acceleration * dt;

        derivative output{};
        output.dx_velocity = state.velocity;
        output.dv_acceleration = acceleration(state, t + dt);
        return output;
    }

    double acceleration(const state& state, double t) {
	    constexpr double k = 15.0f;
        return -k;
    }

    void integrate(state& state, const double t, const double frame_time) {
	    const derivative a = evaluate(state, t, 0.0f, derivative());
        const derivative b = evaluate(state, t, frame_time * 0.5f, a);
        const derivative c = evaluate(state, t, frame_time * 0.5f, b);
        const auto [dx_velocity, dv_acceleration] = evaluate(state, t, frame_time, c);

        const double dx_dt = 1.0f / 6.0f * (a.dx_velocity + 2.0f * (b.dx_velocity + c.dx_velocity) + dx_velocity);
        const double dv_dt = 1.0f / 6.0f * (a.dv_acceleration + 2.0f * (b.dv_acceleration + c.dv_acceleration) + dv_acceleration);

        state.position = state.position + dx_dt * frame_time;
        state.velocity = state.velocity + dv_dt * frame_time;
    }

    time_ctx time_step(const time_ctx& ctx, double frame_time)
    {
        state current_state = ctx.current_state;
        state previous_state = ctx.previous_state;
        double accumulator = ctx.accumulator;
        double t = ctx.time;
        if (frame_time > 0.25) frame_time = 0.25;

        accumulator += frame_time;

        while (accumulator >= delta_time)
        {
            previous_state = current_state;
            integrate(current_state, t, delta_time);
            t += delta_time;
            accumulator -= delta_time;
        }

        const double alpha = accumulator / delta_time;

        const state next_state = current_state * alpha + previous_state * (1.0f - alpha);

        time_ctx new_ctx = {};
        new_ctx.time = t;
        new_ctx.accumulator = accumulator;
        new_ctx.current_state = next_state;
        new_ctx.previous_state = previous_state;
        return new_ctx;
    }
}

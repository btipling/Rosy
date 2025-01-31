#include "Camera.h"
#include "Math.h"
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
	struct state
	{
		double position = 0;
		double velocity = 0;

		state operator*(const double value) const
		{
			state rv{};
			rv.position = position * value;
			rv.velocity = velocity * value;
			return rv;
		}


		state operator+(const double value) const
		{
			state rv{};
			rv.position = position + value;
			rv.velocity = velocity + value;
			return rv;
		}

		state operator*(const state other) const
		{
			state rv{};
			rv.position = position * other.position;
			rv.velocity = velocity * other.velocity;
			return rv;
		}

		state operator+(const state other) const
		{
			state rv{};
			rv.position = position + other.position;
			rv.velocity = velocity + other.velocity;
			return rv;
		}
	};

	struct derivative
	{
		double dx_velocity = 0;      // dx_velocity/dt = velocity
		double dv_acceleration = 0;  // dv_acceleration/dt = acceleration

		derivative operator*(const double  value) const
		{
			derivative rv{};
			rv.dx_velocity = dx_velocity * value;
			rv.dv_acceleration = dv_acceleration * value;
			return rv;
		}

		derivative operator+(const double value) const
		{
			derivative rv{};
			rv.dx_velocity = dx_velocity + value;
			rv.dv_acceleration = dv_acceleration + value;
			return rv;
		}

		derivative operator*(const derivative other) const
		{
			derivative rv{};
			rv.dx_velocity = dx_velocity * other.dx_velocity;
			rv.dv_acceleration = dv_acceleration * other.dv_acceleration;
			return rv;
		}

		derivative operator+(const derivative other) const
		{
			derivative rv{};
			rv.dx_velocity = dx_velocity + other.dx_velocity;
			rv.dv_acceleration = dv_acceleration + other.dv_acceleration;
			return rv;
		}
	};

	constexpr double delta_time = 0.01f;

	struct time_ctx
	{
		double time = 0.f;
		double accumulator = 0.f;
		state previous_state{};
		state current_state{};

	};

	void integrate(state& state, [[maybe_unused]] double t, const double frame_time) {
		state.position = state.position * frame_time;
		state.velocity = state.velocity * frame_time;
	}

	time_ctx time_step([[maybe_unused]] rosy::log const* l, const time_ctx& ctx, double frame_time)
	{
		state current_state = ctx.current_state;
		state previous_state = ctx.previous_state;
		double accumulator = ctx.accumulator;
		double t = ctx.time;
		frame_time = std::min(frame_time, 0.25);

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

	struct movement
	{
		enum direction : uint8_t
		{
			depth,
			horizontal,
			vertical,
		};
		time_ctx step;
		double start;
		direction dir;
	};

	std::array<float, 16> mat4_to_array(glm::mat4 m)
	{
		std::array<float, 16> rv{};
		const auto pos_r = glm::value_ptr(m);
		for (uint64_t i{ 0 }; i < 16; i++) rv[i] = pos_r[i];
		return rv;
	}

	struct synthetic_camera
	{
		SDL_Time start_time;
		SDL_Time last_time;
		rosy::log const* l{ nullptr };
		config cfg{};

		glm::vec3 velocity{ 0.f };
		glm::vec3 position;
		float pitch{ 0.f };
		float yaw{ 0.f };
		std::vector<movement> movements;
		bool go_fast{ false };

		result init()
		{
			if (!SDL_GetCurrentTime(&start_time)) return result::error;
			last_time = start_time;
			movements.reserve(6);
			return result::ok;
		}

		void deinit()
		{
			movements.clear();
		}

		// ReSharper disable once CppMemberFunctionMayBeStatic
		glm::mat4 get_projection([[maybe_unused]] double g, const double s, const double n, const double f, const double fov)
		{
			// Assuming VK NDC in camera for now.
			constexpr auto ndc = glm::mat4(
				glm::vec4(1.f, 0.f, 0.f, 0.f),
				glm::vec4(0.f, -1.f, 0.f, 0.f),
				glm::vec4(0.f, 0.f, 1.f, 0.f),
				glm::vec4(0.f, 0.f, 0.f, 1.f)
			);

			const double h = 1.0 / tan(fov * 0.5);
			const double w = h / s;
			const double a = -n / (f - n);
			const double b = (n * f) / (f - n);

			return ndc * glm::mat4(
				glm::vec4(w, 0, 0, 0),
				glm::vec4(0, h, 0, 0),
				glm::vec4(0, 0, a, 1.f),
				glm::vec4(0, 0, b, 0));
		}

		glm::mat4 get_view_matrix() const
		{
			const glm::mat4 camera_translation = translate(glm::mat4(1.f), position);
			const glm::mat4 camera_rotation = get_rotation_matrix();
			return inverse(camera_translation * camera_rotation);
		}

		glm::mat4 get_rotation_matrix() const
		{
			const glm::quat pitch_rotation = angleAxis(pitch, glm::vec3{ 1.f, 0.f, 0.f });
			const glm::quat yaw_rotation = angleAxis(yaw, glm::vec3{ 0.f, -1.f, 0.f });

			return toMat4(yaw_rotation) * toMat4(pitch_rotation);
		}

		void integrate_all()
		{
			const double base_velocity = go_fast ? 0.1 : 0.025;
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

		void integrate(const movement::direction direction, const double v)
		{
			bool found = false;
			for (size_t i{ 0 }; i < movements.size(); i++)
			{
				if (movement mv = movements[i]; mv.dir == direction)
				{
					mv.step.current_state.velocity = v;
					movements[i] = mv;
					found = true;
				}
			}
			if (!found) {
				movements.push_back({
					.step = {
						.time = 0.f,
						.accumulator = 0.f,
						.previous_state = {},
						.current_state = {
							.position = 0.05f * v,
							.velocity = v,
						},
					},
					.start = 0.f,
					.dir = direction,
					});
			}
		}

		result update()
		{
			integrate_all();
			int to_remove{ -1 };
			const glm::mat4 camera_rotation = get_rotation_matrix();


			glm::vec4 vel = { 0.f, 0.f, 0.f, 0.f };
			SDL_Time tick = 0;
			if (!SDL_GetCurrentTime(&tick))
			{
				l->error(std::format("Error getting current SDL tick: {}", SDL_GetError()));
				return result::error;
			}
			const SDL_Time frame_time = tick - last_time;
			last_time = tick;
			const auto dt = static_cast<double>(frame_time);
			bool updated = false;
			for (size_t i{ 0 }; i < movements.size(); i++)
			{
				movement mv = movements[i];
				const auto new_step = time_step(l, mv.step, dt);
				movements[i].step = new_step;
				if (is_equal(new_step.current_state.velocity, 0.0))
				{
					to_remove = static_cast<int>(i);
				}
				switch (mv.dir)
				{
				case movement::horizontal:
					updated = true;
					vel[0] = static_cast<float>(mv.step.current_state.position);
					break;
				case movement::vertical:
					updated = true;
					vel[1] = static_cast<float>(mv.step.current_state.position);
					break;
				case movement::direction::depth:
					updated = true;
					vel[2] = static_cast<float>(mv.step.current_state.position);
					break;
				}
			}
			if (to_remove > -1) movements.erase(movements.begin() + to_remove);
			if (!updated) return result::ok;

			position += glm::vec3(camera_rotation * vel);
			return result::ok;
		}

		result process_sdl_event(const SDL_Event& event, const bool cursor_enabled)
		{
			if (event.type == SDL_EVENT_KEY_DOWN) {
				if (event.key.key == SDLK_W) { velocity.z = 1.f; }
				if (event.key.key == SDLK_S) { velocity.z = -1.f; }
				if (event.key.key == SDLK_A) { velocity.x = -1.f; }
				if (event.key.key == SDLK_D) { velocity.x = 1.f; }
				if (event.key.key == SDLK_SPACE) { velocity.y = 1.f; }
				if (event.key.key == SDLK_Z) { velocity.y = -1.f; }
				if (event.key.mod & SDL_KMOD_SHIFT) { go_fast = true; }
			}

			if (event.type == SDL_EVENT_KEY_UP) {
				if (event.key.key == SDLK_W) { velocity.z = 0; }
				if (event.key.key == SDLK_S) { velocity.z = 0; }
				if (event.key.key == SDLK_A) { velocity.x = 0; }
				if (event.key.key == SDLK_D) { velocity.x = 0; }
				if (event.key.key == SDLK_SPACE) { velocity.y = 0; }
				if (event.key.key == SDLK_Z) { velocity.y = 0; }
				if (!(event.key.mod & SDL_KMOD_SHIFT)) { go_fast = false; }
			}
			if (!cursor_enabled) return result::ok;
			if (event.type == SDL_EVENT_MOUSE_MOTION) {
				yaw -= event.motion.xrel / 500.f;
				pitch += event.motion.yrel / 500.f;
			}
			update();
			return result::ok;
		}
	};
	synthetic_camera* sc{ nullptr };
}

result camera::init(log const* new_log, const config cfg)
{
	if (new_log == nullptr)
	{
		return result::invalid_argument;
	}
	{
		l = new_log;
		l->info("Camera initializing");
	} {
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
		if (const auto res = sc->init(); res != result::ok)
		{
			l->error("graphics_device initialization failed");
			return result::graphics_init_failure;
		}

		v = mat4_to_array(sc->get_view_matrix());
	}
	return result::ok;
}

void camera::deinit() const
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
result camera::update(const uint32_t viewport_width, const uint32_t viewport_height)
{
	if (const auto res = sc->update(); res != result::ok)
	{
		return res;
	}
	const float w = static_cast<float>(viewport_width);
	const float h = static_cast<float>(viewport_height);
	const auto proj = sc->get_projection(g, w / h, n, f, glm::radians(fov));
	const auto view = sc->get_view_matrix();
	p = mat4_to_array(proj);
	v = mat4_to_array(view);
	vp = mat4_to_array(proj * view);
	r = mat4_to_array(sc->get_rotation_matrix());
	const auto pos_r = glm::value_ptr(sc->position);
	for (uint64_t i{0}; i < 3; i += 1) position[i] = pos_r[i];

	return result::ok;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
result camera::process_sdl_event(const SDL_Event& event, const bool cursor_enabled)
{
	if (const auto res = sc->process_sdl_event(event, cursor_enabled); res != result::ok) {
		return res;
	}
	return result::ok;
}

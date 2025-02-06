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
			state rv;
			rv.position = position * value;
			rv.velocity = velocity * value;
			return rv;
		}


		state operator+(const double value) const
		{
			state rv;
			rv.position = position + value;
			rv.velocity = velocity + value;
			return rv;
		}

		state operator*(const state other) const
		{
			state rv;
			rv.position = position * other.position;
			rv.velocity = velocity * other.velocity;
			return rv;
		}

		state operator+(const state other) const
		{
			state rv;
			rv.position = position + other.position;
			rv.velocity = velocity + other.velocity;
			return rv;
		}
	};

	state time_step([[maybe_unused]] rosy::log const* l, state& current_state, const double frame_time)
	{
		current_state.position += current_state.velocity * frame_time;
		current_state.velocity = 0;
		return current_state;
	}


	struct movement
	{
		enum direction : uint8_t
		{
			depth,
			horizontal,
			vertical,
		};
		state current_state;
		direction dir;
	};

	std::array<float, 16> mat4_to_array(glm::mat4 m)
	{
		std::array<float, 16> rv;
		const auto pos_r = glm::value_ptr(m);
		for (uint64_t i{ 0 }; i < 16; i++) rv[i] = pos_r[i];
		return rv;
	}

	struct synthetic_camera
	{
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
			double base_velocity = 5.0;
			base_velocity = go_fast ? base_velocity * 2.0 : base_velocity;
			if (velocity.x != 0)
			{
				integrate(movement::direction::horizontal, std::abs(velocity.x) * base_velocity);
			}
			if (velocity.y != 0)
			{
				integrate(movement::direction::vertical, std::abs(velocity.y) * base_velocity);
			}
			if (velocity.z != 0)
			{
				integrate(movement::direction::depth, std::abs(velocity.z) * base_velocity);
			}
		}

		void integrate(const movement::direction direction, const double v)
		{
			bool found = false;
			for (size_t i{ 0 }; i < movements.size(); i++)
			{
				if (movement mv = movements[i]; mv.dir == direction)
				{
					mv.current_state.velocity = v;
					movements[i] = mv;
					found = true;
				}
			}
			if (!found) {
				movements.push_back({
					.current_state = {
						.position = 0,
						.velocity = v,
					},
					.dir = direction,
					});
			}
		}

		result update(const uint64_t dt)
		{
			integrate_all();
			int to_remove{ -1 };
			const glm::mat4 camera_rotation = get_rotation_matrix();


			float vel_x{ 0.f };
			float vel_y{ 0.f };
			float vel_z{ 0.f };
			SDL_Time tick = 0;
			if (!SDL_GetCurrentTime(&tick))
			{
				return result::error;
			}
			bool updated = false;
			for (size_t i{ 0 }; i < movements.size(); i++)
			{
				auto [step, dir] = movements[i];
				step.position = 0;
				const auto current_state = time_step(l, step, static_cast<double>(dt) / sdl_time_to_seconds);
				movements[i].current_state = current_state;
				if (current_state.velocity < 0.0 || is_equal(current_state.velocity, 0.0, 0.01))
				{
					to_remove = static_cast<int>(i);
				}
				switch (dir)
				{
				case movement::horizontal:
					updated = true;
					vel_x = static_cast<float>(current_state.position) * velocity.x;
					break;
				case movement::vertical:
					updated = true;
					vel_y = static_cast<float>(current_state.position) * velocity.y;
					break;
				case movement::direction::depth:
					updated = true;
					vel_z = static_cast<float>(current_state.position) * velocity.z;
					break;
				}
			}
			if (to_remove > -1) movements.erase(movements.begin() + to_remove);
			if (!updated) return result::ok;

			position += glm::vec3(camera_rotation * glm::vec4(vel_x, vel_y, vel_z, 0.f));
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
result camera::update(const uint32_t viewport_width, const uint32_t viewport_height, const uint64_t dt)
{
	if (const auto res = sc->update(dt); res != result::ok)
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
	yaw = sc->yaw;
	pitch = sc->pitch;
	const auto pos_r = glm::value_ptr(sc->position);
	for (uint64_t i{ 0 }; i < 3; i += 1) position[i] = pos_r[i];

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

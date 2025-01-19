#include "Camera.h"

using namespace rosy;

namespace
{
	struct synthetic_camera
	{
		rosy::log const* l{ nullptr };
		config cfg{};
		double g;
		double s;
		double n;
		double f;
		double fov;
		std::array<float, 16> p; // projection
		std::array<float, 16> v; // view
		std::array<float, 16> vp; // view projection
		std::array<float, 16> r; // camera rotation

		result init()
		{
			return result::ok;
		}

		void deinit()
		{
		}

		result process_sdl_event([[maybe_unused]] SDL_Event event)
		{
			return result::ok;
		}
	};
	synthetic_camera* sc{ nullptr };
}

result camera::init(log const* new_log, config cfg)
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
		if (const auto res = sc->init(); res != result::ok)
		{
			l->error("graphics_device initialization failed");
			return result::graphics_init_failure;
		}
		g = sc->g;
		s = sc->s;
		n = sc->f;
		fov = sc->fov;
		p = sc->p;
		v = sc->v;
		vp = sc->vp;
		r = sc->r;
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

result camera::process_sdl_event(const SDL_Event& event)
{
	if (const auto res = sc->process_sdl_event(event); res != result::ok) {
		return res;
	}
	p = sc->p;
	v = sc->v;
	vp = sc->vp;
	r = sc->r;
	return result::ok;
}

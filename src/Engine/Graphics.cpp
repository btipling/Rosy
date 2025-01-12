#include "Graphics.h"

#include <format>

#include "volk/volk.h"
#include "vma/vk_mem_alloc.h"

using namespace rosy;

namespace {
	/// Graphics Device
	constexpr  uint32_t graphics_created_bit_instance         = 0b00000000000000000000000000000001;
	constexpr  uint32_t graphics_created_bit_device           = 0b00000000000000000000000000000010;
	constexpr  uint32_t graphics_created_bit_surface          = 0b00000000000000000000000000000100;
	constexpr  uint32_t graphics_created_bit_vma              = 0b00000000000000000000000000001000;
	constexpr  uint32_t graphics_created_bit_debug_messenger  = 0b00000000000000000000000000010000;
	constexpr  uint32_t graphics_created_bit_fence            = 0b00000000000000000000000000100000;
	constexpr  uint32_t graphics_created_bit_command_pool     = 0b00000000000000000000000001000000;
	constexpr  uint32_t graphics_created_bit_draw_image       = 0b00000000000000000000000010000000;
	constexpr  uint32_t graphics_created_bit_depth_image      = 0b00000000000000000000000100000000;
	constexpr  uint32_t graphics_created_bit_semaphore        = 0b00000000000000000000001000000000;
	constexpr  uint32_t graphics_created_bit_swapchain        = 0b00000000000000000000010000000000;
	constexpr  uint32_t graphics_created_bit_ktx              = 0b00000000000000000000100000000000;
	constexpr  uint32_t graphics_created_bit_descriptor       = 0b00000000000000000001000000000000;

	struct graphics_device
	{
		rosy::log const* l{ nullptr };
		uint32_t graphics_created_bitmask{0};
		SDL_Window* window;

		result init()
		{
			VkResult vk_result = volkInitialize();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to initialize volk! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = query_instance_layers();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to query instance layers! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = query_instance_extensions();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to query instance extensions! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_instance();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to create Vulkan instance! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = create_debug_callback();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to create Vulkan debug callback! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_surface();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to create surface! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_physical_device();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to create Vulkan physical device! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = query_device_layers();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to query device layers! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = query_device_extensions();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to query device extensions! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_device();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to create Vulkan device {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_tracy();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init tracy! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_allocator();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init vma! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_presentation_queue();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to get presentation queue! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_swap_chain();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init swap chain! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_draw_image();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init draw image! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_descriptors();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init draw descriptors! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_command_pool();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init command pool! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_command_buffers();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init command buffers! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_sync_objects();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init sync objects! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_ui();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init UI! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_csm_image();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init csm! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_commands();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init commands! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_data();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init data! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_ktx();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init ktx! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}
			return result::ok;
		}

		void deinit()
		{
			// Deinit acquired resources.
		}

		VkResult query_instance_layers()
		{
			l->info("Querying instance layers");
			return VK_SUCCESS;
		}

		VkResult query_instance_extensions()
		{
			l->info("Querying instance extensions");
			return VK_SUCCESS;
		}

		VkResult init_instance()
		{
			l->info("Initializing instance");
			graphics_created_bitmask |= graphics_created_bit_instance;
			return VK_SUCCESS;
		}

		VkResult create_debug_callback()
		{
			l->info("Creating Vulkan debug callback");
			graphics_created_bitmask |= graphics_created_bit_debug_messenger;
			return VK_SUCCESS;
		}

		VkResult init_surface()
		{
			l->info("Initializing SDL Surface");
			graphics_created_bitmask |= graphics_created_bit_surface;
			return VK_SUCCESS;
		}

		VkResult init_physical_device()
		{
			l->info("Initializing physical device");
			return VK_SUCCESS;
		}

		VkResult query_device_layers()
		{
			l->info("Querying device layers");
			return VK_SUCCESS;
		}

		VkResult query_device_extensions()
		{
			l->info("Querying device extensions");
			return VK_SUCCESS;
		}

		VkResult init_device()
		{
			l->info("Initializing device");
			graphics_created_bitmask |= graphics_created_bit_device;
			return VK_SUCCESS;
		}

		VkResult init_tracy()
		{
			l->info("Initializing Tracy");
			return VK_SUCCESS;
		}

		VkResult init_allocator()
		{
			l->info("Initializing VMA");
			graphics_created_bitmask |= graphics_created_bit_vma;
			return VK_SUCCESS;
		}

		VkResult init_presentation_queue()
		{
			l->info("Initializing presentation queue");
			return VK_SUCCESS;
		}

		VkResult init_swap_chain()
		{
			l->info("Initializing swap chain");
			graphics_created_bitmask |= graphics_created_bit_swapchain;
			return VK_SUCCESS;
		}

		VkResult init_draw_image()
		{
			l->info("Initializing draw image");
			graphics_created_bitmask |= graphics_created_bit_draw_image;
			return VK_SUCCESS;
		}

		VkResult init_descriptors()
		{
			l->info("Initializing descriptors");
			graphics_created_bitmask |= graphics_created_bit_descriptor;
			return VK_SUCCESS;
		}

		VkResult init_command_pool()
		{
			l->info("Initializing command pool");
			graphics_created_bitmask |= graphics_created_bit_command_pool;
			return VK_SUCCESS;
		}

		VkResult init_command_buffers()
		{
			l->info("Initializing command buffer");
			return VK_SUCCESS;
		}

		VkResult init_sync_objects()
		{
			l->info("Initializing sync objects");
			return VK_SUCCESS;
		}

		VkResult init_ui()
		{
			l->info("Initializing UI");
			return VK_SUCCESS;
		}

		VkResult init_csm_image()
		{
			l->info("Initializing cascading shadow map");
			graphics_created_bitmask |= graphics_created_bit_depth_image;
			return VK_SUCCESS;
		}

		VkResult init_commands()
		{
			l->info("Initializing commands");
			return VK_SUCCESS;
		}

		VkResult init_data()
		{
			l->info("Initializing data");
			return VK_SUCCESS;
		}

		VkResult init_ktx()
		{
			l->info("Initializing ktx");
			graphics_created_bitmask |= graphics_created_bit_ktx;
			return VK_SUCCESS;
		}
	};

	graphics_device* gd{ nullptr };
}


//// Graphics

result graphics::init(SDL_Window* new_window, log const* new_log)
{
	if (!new_window || !new_log)
	{
		return result::invalid_argument;
	}
	{
		l = new_log;
		l->info("Graphics init start");
	}
	{
		// Init graphics device
		gd = new(std::nothrow) graphics_device{};
		if (gd == nullptr)
		{
			l->error("graphics_device allocation failed");
			return result::allocation_failure;
		}
		gd->l = new_log;
		gd->window = new_window;
		if (const auto res = gd->init(); res != result::ok)
		{
			l->error("graphics_device initialization failed");
			return result::graphics_init_failure;
		}
	}

	l->info("Graphics init done");


	return result::ok;
}

void graphics::deinit()
{
	l->info("Graphics deinit start");

	if (gd)
	{
		gd->deinit();
		delete gd;
		gd = nullptr;
	}

	l->info("Graphics deinit end");
	l = nullptr;
}


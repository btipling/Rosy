#include "Graphics.h"

#include <format>
#include <vector>

#include "volk/volk.h"
#include "vma/vk_mem_alloc.h"
#include <SDL3/SDL_vulkan.h>

using namespace rosy;

namespace {
	/// Graphics Device

	constexpr  uint32_t graphics_created_bit_instance        = 0b00000000000000000000000000000001;
	constexpr  uint32_t graphics_created_bit_device	         = 0b00000000000000000000000000000010;
	constexpr  uint32_t graphics_created_bit_surface         = 0b00000000000000000000000000000100;
	constexpr  uint32_t graphics_created_bit_vma             = 0b00000000000000000000000000001000;
	constexpr  uint32_t graphics_created_bit_debug_messenger = 0b00000000000000000000000000010000;
	constexpr  uint32_t graphics_created_bit_fence           = 0b00000000000000000000000000100000;
	constexpr  uint32_t graphics_created_bit_command_pool    = 0b00000000000000000000000001000000;
	constexpr  uint32_t graphics_created_bit_draw_image      = 0b00000000000000000000000010000000;
	constexpr  uint32_t graphics_created_bit_depth_image     = 0b00000000000000000000000100000000;
	constexpr  uint32_t graphics_created_bit_semaphore       = 0b00000000000000000000001000000000;
	constexpr  uint32_t graphics_created_bit_swapchain       = 0b00000000000000000000010000000000;
	constexpr  uint32_t graphics_created_bit_ktx             = 0b00000000000000000000100000000000;
	constexpr  uint32_t graphics_created_bit_descriptor      = 0b00000000000000000001000000000000;

	const char* default_instance_layers[] = {
		"VK_LAYER_LUNARG_api_dump",
		"VK_LAYER_KHRONOS_validation",
		"VK_LAYER_KHRONOS_shader_object",
		"VK_LAYER_KHRONOS_synchronization2",
	};

	const char* default_instance_extensions[] = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	};

	rosy::log const* debug_callback_logger = nullptr; // Exists only for the purpose of the callback, this is also not thread safe.
	VkBool32 VKAPI_CALL debug_callback(
		[[maybe_unused]] const VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
		[[maybe_unused]] const VkDebugUtilsMessageSeverityFlagsEXT message_type,
		const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
		[[maybe_unused]] void* p_user_data)
	{
		if (!debug_callback_logger) return VK_FALSE;
		if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			debug_callback_logger->error(std::format("[{}] {}",
				message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ? "Validation" : "Performance",
				p_callback_data->pMessage));
		}
		else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			debug_callback_logger->warn(std::format("[{}] {}",
				message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ? "Validation" : "Performance",
				p_callback_data->pMessage));
		}
		else {
			debug_callback_logger->debug(std::format("[{}] {}",
				message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ? "Validation" : "Performance",
				p_callback_data->pMessage));
		}
		return VK_FALSE;
	}

	struct graphics_device
	{
		rosy::log const* l{ nullptr };
		uint32_t graphics_created_bitmask{ 0 };
		bool enable_validation_layers{ true };
		std::vector<const char*> instance_layer_properties;
		std::vector<const char*> device_layer_properties;
		std::vector<const char*> instance_extensions;
		std::vector<const char*> device_device_extensions;

		VkInstance instance;
		VkDevice device;
		VkPhysicalDevice physical_device;
		VmaAllocator allocator;

		VkDebugUtilsMessengerEXT debug_messenger;
		VkSurfaceKHR surface;


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

		void deinit() const
		{
			// Deinit acquired resources in the opposite order in which they were created

			if (graphics_created_bitmask & graphics_created_bit_surface)
			{
				SDL_Vulkan_DestroySurface(instance, surface, nullptr);
			}

			if (graphics_created_bitmask & graphics_created_bit_debug_messenger)
			{
				vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
			}

			if (graphics_created_bitmask & graphics_created_bit_instance)
			{
				vkDestroyInstance(instance, nullptr);
			}
		}

		VkResult query_instance_layers()
		{
			l->info("Querying instance layers");
			VkResult result;

			uint32_t p_property_count = 0;
			{
				if (result = vkEnumerateInstanceLayerProperties(&p_property_count, nullptr); result != VK_SUCCESS) return result;
				l->debug(std::format("Found %d instance layers {}", p_property_count));
				if (p_property_count == 0) return result;
			}

			std::vector<VkLayerProperties> layers;
			{
				layers.resize(p_property_count);
				if (result = vkEnumerateInstanceLayerProperties(&p_property_count, layers.data()); result != VK_SUCCESS) return result;
				if (!enable_validation_layers) return result;
			}

			for (VkLayerProperties lp : layers)
			{
				l->debug(std::format("Instance layer name: {} layer description: {}", lp.layerName, lp.description));
				for (const char* layer_name : default_instance_layers)
				{
					if (strcmp(layer_name, lp.layerName) == 0)
					{
						l->debug(std::format("Adding instance layer: {}", lp.layerName));
						instance_layer_properties.push_back(layer_name);
					}
				}
			}
			return result;
		}

		VkResult query_instance_extensions()
		{
			l->info("Querying instance extensions");
			VkResult result;

			uint32_t p_property_count = 0;
			{
				if (result = vkEnumerateInstanceExtensionProperties(nullptr, &p_property_count, nullptr); result != VK_SUCCESS) return result;
				l->debug(std::format("Found {} instance extensions", p_property_count));
				if (p_property_count == 0) return result;
			}

			std::vector<VkExtensionProperties> extensions;
			{
				extensions.resize(p_property_count);
				if (result = vkEnumerateInstanceExtensionProperties(nullptr, &p_property_count, extensions.data()); result != VK_SUCCESS) return result;
				l->debug(std::format("num required instance extensions: {}", std::size(default_instance_extensions)));
			}

			{
				// Setup required instance extensions
				uint32_t extension_count;
				const auto extension_names = SDL_Vulkan_GetInstanceExtensions(&extension_count);
				for (uint32_t i = 0; i < extension_count; i++)
				{
					l->debug(std::format("pushing back required SDL instance extension with name: {}", extension_names[i]));
					instance_extensions.push_back(extension_names[i]);
				}
				for (uint32_t i = 0; i < std::size(default_instance_extensions); i++)
				{
					l->debug(std::format("pushing back required rosy instance extension with name: {}", default_instance_extensions[i]));
					instance_extensions.push_back(default_instance_extensions[i]);
				}
			}
			l->debug(std::format("num instanceExtensions: {}", instance_extensions.size()));

			std::vector<const char*> required_instance_extensions(std::begin(instance_extensions), std::end(instance_extensions));

			// ReSharper disable once CppUseStructuredBinding
			for (VkExtensionProperties properties : extensions)
			{
				l->debug(std::format("Instance extension name: {}", properties.extensionName));
				for (const char* extension_name : instance_extensions)
				{
					if (strcmp(extension_name, properties.extensionName) == 0)
					{
						l->debug(std::format("Requiring instance extension: {}", extension_name));
						std::erase(required_instance_extensions, extension_name);
					}
				}
			}
			if (required_instance_extensions.size() != 0) return VK_ERROR_EXTENSION_NOT_PRESENT;
			return result;
		}

		VkResult init_instance()
		{
			l->info("Initializing instance");

			VkApplicationInfo app_info{
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pApplicationName = "Rosy",
				.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
				.pEngineName = "Rosy",
				.engineVersion = VK_MAKE_VERSION(1, 0, 0),
				.apiVersion = VK_API_VERSION_1_3,
			};

			constexpr VkDebugUtilsMessengerCreateInfoEXT create_debug_callback_info_ext = {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				.pNext = nullptr,
				.flags = 0,
				.messageSeverity =
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
				.messageType =
					VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
				.pfnUserCallback = debug_callback,
				.pUserData = nullptr,
			};

			const VkInstanceCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pNext = &create_debug_callback_info_ext,
				.pApplicationInfo = &app_info,
				.enabledLayerCount = static_cast<uint32_t>(instance_layer_properties.size()),
				.ppEnabledLayerNames = instance_layer_properties.data(),
				.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size()),
				.ppEnabledExtensionNames = instance_extensions.data(),
			};

			const VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
			if (result != VK_SUCCESS) return result;
			l->debug("Vulkan instance created successfully!");
			volkLoadInstance(instance);

			// Set the debug callback logger to use the graphics device logger.
			debug_callback_logger = l;
			graphics_created_bitmask |= graphics_created_bit_instance;
			return result;
		}

		VkResult create_debug_callback()
		{
			l->info("Creating Vulkan debug callback");
			if (!enable_validation_layers) return VK_SUCCESS;

			constexpr VkDebugUtilsMessengerCreateInfoEXT create_debug_callback_info_ext = {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				.pNext = nullptr,
				.flags = 0,
				.messageSeverity =
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
				.messageType =
					VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
				.pfnUserCallback = debug_callback,
				.pUserData = nullptr,
			};
			const VkResult result = vkCreateDebugUtilsMessengerEXT(instance, &create_debug_callback_info_ext, nullptr, &debug_messenger);
			if (result != VK_SUCCESS) return result;
			graphics_created_bitmask |= graphics_created_bit_debug_messenger;
			return result;
		}

		VkResult init_surface()
		{
			l->info("Initializing SDL Surface");
			if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface))
			{
				l->error(std::format("Failed to create SDL Vulkan surface: {}", SDL_GetError()));
				return VK_ERROR_INITIALIZATION_FAILED;
			}
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


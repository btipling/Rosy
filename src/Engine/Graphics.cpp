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

	const char* default_device_extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef PROFILING_ENABLED
		VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
#endif
		VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
		VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME,
		//VK_KHR_MULTIVIEW_EXTENSION_NAME,
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

	struct swap_chain_support_details {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};

	struct graphics_device
	{
		rosy::log const* l{ nullptr };
		uint32_t graphics_created_bitmask{ 0 };
		bool enable_validation_layers{ true };
		std::vector<const char*> instance_layer_properties;
		std::vector<const char*> device_layer_properties;
		std::vector<const char*> instance_extensions;
		std::vector<const char*> device_extensions;

		VkInstance instance;
		VkDevice device;
		VkPhysicalDevice physical_device;
		VmaAllocator allocator;

		VkDebugUtilsMessengerEXT debug_messenger;
		VkSurfaceKHR surface;
		VkPhysicalDeviceFeatures required_features{};

		swap_chain_support_details swapchain_details = {};

		VkPhysicalDeviceProperties physical_device_properties;
		VkPhysicalDeviceFeatures supported_features;
		VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
		std::vector<VkQueueFamilyProperties> queue_family_properties;
		uint32_t queue_count = 0;
		uint32_t queue_index = 0;
		std::vector<float> queue_priorities;

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

			if (graphics_created_bitmask & graphics_created_bit_device)
			{
				if (const VkResult result = vkDeviceWaitIdle(device); result == VK_SUCCESS) vkDestroyDevice(device, nullptr);
			}

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
			std::vector<VkPhysicalDevice> physical_devices;

			uint32_t physical_device_count = 0;
			VkResult result = vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);

			physical_devices.resize(physical_device_count);
			vkEnumeratePhysicalDevices(instance, &physical_device_count, &physical_devices[0]);

			// Shader features
			required_features.multiDrawIndirect  = VK_TRUE;
			required_features.tessellationShader = VK_TRUE;
			required_features.geometryShader     = VK_TRUE;
			required_features.shaderInt64        = VK_TRUE;

			// Rendering features
			required_features.fillModeNonSolid   = VK_TRUE;
			required_features.wideLines          = VK_TRUE;

			// Depth features
			required_features.depthBiasClamp     = VK_TRUE;
			required_features.depthClamp         = VK_TRUE;
			required_features.depthBounds        = VK_TRUE;

			uint32_t new_queue_count = 0;
			for (const VkPhysicalDevice& p_device : physical_devices)
			{
				// get device properties
				VkPhysicalDeviceProperties device_properties;
				vkGetPhysicalDeviceProperties(p_device, &device_properties);

				bool swap_chain_adequate = false;
				swap_chain_support_details swap_chain_support = query_swap_chain_support(p_device);
				swap_chain_adequate = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();
				if (!swap_chain_adequate) continue;

				// shader objects required
				VkPhysicalDeviceShaderObjectFeaturesEXT shader_object_features{};
				shader_object_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
				shader_object_features.pNext = nullptr;

				VkPhysicalDeviceDepthClipEnableFeaturesEXT  depth_clip_features{};
				depth_clip_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT;
				depth_clip_features.pNext = &shader_object_features;

				VkPhysicalDeviceFeatures2 device_features2{};
				device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				device_features2.pNext = &depth_clip_features;
				vkGetPhysicalDeviceFeatures2(p_device, &device_features2);

				if (!shader_object_features.shaderObject) continue;

				// dynamic rendering required
				VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features{};
				buffer_device_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
				buffer_device_address_features.pNext = nullptr;

				device_features2 = {};
				device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				device_features2.pNext = &buffer_device_address_features;
				vkGetPhysicalDeviceFeatures2(p_device, &device_features2);

				if (!buffer_device_address_features.bufferDeviceAddress) continue;


				// data device address required
				VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features{};
				dynamic_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
				dynamic_rendering_features.pNext = nullptr;

				device_features2 = {};
				device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				device_features2.pNext = &dynamic_rendering_features;
				vkGetPhysicalDeviceFeatures2(p_device, &device_features2);

				if (!dynamic_rendering_features.dynamicRendering) continue;

				// features
				VkPhysicalDeviceFeatures features;
				vkGetPhysicalDeviceFeatures(p_device, &features);
				if (features.multiDrawIndirect != VK_TRUE) continue;
				if (features.tessellationShader != VK_TRUE) continue;
				if (features.geometryShader != VK_TRUE) continue;
				if (features.shaderInt64 != VK_TRUE) continue;

				if (features.fillModeNonSolid != VK_TRUE) continue;
				if (features.wideLines != VK_TRUE) continue;

				if (features.depthBiasClamp != VK_TRUE) continue;
				if (features.depthClamp != VK_TRUE) continue;
				if (features.depthBounds != VK_TRUE) continue;

				// memory
				VkPhysicalDeviceMemoryProperties mem_props;
				vkGetPhysicalDeviceMemoryProperties(p_device, &mem_props);

				// queues
				uint32_t new_queue_index = 0;
				std::vector<VkQueueFamilyProperties> current_queue_family_properties_data;
				{
					vkGetPhysicalDeviceQueueFamilyProperties(p_device, &new_queue_count, nullptr);
					current_queue_family_properties_data.resize(new_queue_count);
					vkGetPhysicalDeviceQueueFamilyProperties(p_device, &new_queue_count, &current_queue_family_properties_data[0]);
				}

				for (std::uint32_t i = 0; i < current_queue_family_properties_data.size(); ++i)
				{
					VkQueueFamilyProperties q_props = current_queue_family_properties_data[i];
					if (q_props.timestampValidBits < 64) continue;

					if (!(q_props.queueFlags & (
						VK_QUEUE_GRAPHICS_BIT |
						VK_QUEUE_COMPUTE_BIT |
						VK_QUEUE_TRANSFER_BIT |
						VK_QUEUE_SPARSE_BINDING_BIT))) continue;

					VkBool32 present_support = false;
					vkGetPhysicalDeviceSurfaceSupportKHR(p_device, i, surface, &present_support);
					if (!present_support) continue;
					// Get the physical device with the largest queue count if all other requirements have been met.
					if (q_props.queueCount > new_queue_count)
					{
						new_queue_index = i;
						new_queue_count = q_props.queueCount;
						physical_device = p_device;
						physical_device_properties = device_properties;
						supported_features = features;
						physical_device_memory_properties = mem_props;
						queue_index = new_queue_index;
						queue_count = new_queue_count;
						queue_family_properties = current_queue_family_properties_data;
					}
				}
			}
			l->debug("Vulkan physical device created successfully!");
			return result;
		}

		[[nodiscard]] VkResult query_device_layers() const
		{
			l->info("Querying device layers");
			uint32_t p_property_count = 0;
			VkResult result = vkEnumerateDeviceLayerProperties(physical_device, &p_property_count, nullptr);
			if (result != VK_SUCCESS) return result;
			l->debug(std::format("Found {} device layers", p_property_count));
			if (p_property_count == 0) return result;
			std::vector<VkLayerProperties> layers;
			layers.resize(p_property_count);
			result = vkEnumerateDeviceLayerProperties(physical_device, &p_property_count, layers.data());
			if (result != VK_SUCCESS) return result;
			for (VkLayerProperties lp : layers)
			{
				l->debug(std::format("Device layer name: {} layer description: {}", lp.layerName, lp.description));
			}
			return result;
		}

		VkResult query_device_extensions()
		{
			l->info("Querying device extensions");

			uint32_t p_property_count = 0;

			VkResult result = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &p_property_count, nullptr);
			if (result != VK_SUCCESS) return result;

			l->debug(std::format("Found {} device extensions", p_property_count));
			if (p_property_count == 0) return result;

			std::vector<VkExtensionProperties> extensions;
			extensions.resize(p_property_count);

			result = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &p_property_count, extensions.data());
			if (result != VK_SUCCESS) return result;

			// validate required device extensions
			std::vector<const char*> required_device_extensions(std::begin(default_device_extensions), std::end(default_device_extensions));

			for (auto [extensionName, specVersion] : extensions)
			{
				l->debug(std::format("Device extension name: {}", extensionName));
				for (const char* extension_name : default_device_extensions)
				{
					if (strcmp(extension_name, extensionName) == 0)
					{
						l->debug(std::format("Requiring device extension: {}", extension_name));
						device_extensions.push_back(extension_name);
						std::erase(required_device_extensions, extension_name);
					}
				}
			}

			if (required_device_extensions.size() != 0)
			{
				return VK_ERROR_EXTENSION_NOT_PRESENT;
			}
			return result;
		}

		VkResult init_device()
		{
			l->info("Initializing device");

			VkDeviceQueueCreateInfo device_queue_create_info{};
			device_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			device_queue_create_info.pNext = nullptr;
			device_queue_create_info.flags = 0;
			device_queue_create_info.queueFamilyIndex = queue_index;
			queue_priorities.resize(queue_count, 0.5f);
			device_queue_create_info.pQueuePriorities = queue_priorities.data();
			device_queue_create_info.queueCount = queue_count;

			VkPhysicalDeviceVulkan13Features vulkan13_features{};
			vulkan13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
			vulkan13_features.pNext = nullptr;
			vulkan13_features.dynamicRendering = VK_TRUE;
			vulkan13_features.synchronization2 = VK_TRUE;
			vulkan13_features.maintenance4 = VK_TRUE;

			VkPhysicalDeviceVulkan12Features vulkan12_features{};
			vulkan12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
			vulkan12_features.pNext = &vulkan13_features;
			vulkan12_features.bufferDeviceAddress = VK_TRUE;
			vulkan12_features.descriptorIndexing = VK_TRUE;
			vulkan12_features.shaderInputAttachmentArrayDynamicIndexing = VK_TRUE;
			vulkan12_features.shaderUniformTexelBufferArrayDynamicIndexing = VK_TRUE;
			vulkan12_features.shaderStorageTexelBufferArrayDynamicIndexing = VK_TRUE;
			vulkan12_features.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
			vulkan12_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
			vulkan12_features.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
			vulkan12_features.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
			vulkan12_features.shaderUniformTexelBufferArrayNonUniformIndexing = VK_TRUE;
			vulkan12_features.shaderStorageTexelBufferArrayNonUniformIndexing = VK_TRUE;
			vulkan12_features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
			vulkan12_features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
			vulkan12_features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
			vulkan12_features.descriptorBindingUniformTexelBufferUpdateAfterBind = VK_TRUE;
			vulkan12_features.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
			vulkan12_features.descriptorBindingPartiallyBound = VK_TRUE;
			vulkan12_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
			vulkan12_features.runtimeDescriptorArray = VK_TRUE;
			vulkan12_features.samplerFilterMinmax = VK_TRUE;
			vulkan12_features.scalarBlockLayout = VK_TRUE;
			vulkan12_features.imagelessFramebuffer = VK_TRUE;
			vulkan12_features.uniformBufferStandardLayout = VK_TRUE;
			vulkan12_features.shaderSubgroupExtendedTypes = VK_TRUE;
#ifdef PROFILING_ENABLED
			vulkan12_features.hostQueryReset = VK_TRUE;
#endif

			VkPhysicalDeviceVulkan11Features vulkan11_features{};
			vulkan11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
			vulkan11_features.pNext = &vulkan12_features;
			vulkan11_features.variablePointers = VK_TRUE;
			vulkan11_features.variablePointersStorageBuffer = VK_TRUE;
			vulkan11_features.multiview = VK_FALSE;
			vulkan11_features.multiviewGeometryShader = VK_FALSE;
			vulkan11_features.multiviewTessellationShader = VK_FALSE;

			VkPhysicalDeviceShaderObjectFeaturesEXT enable_shader_object = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,
				.pNext = &vulkan11_features,
				.shaderObject = VK_TRUE
			};

			VkPhysicalDeviceDepthClipEnableFeaturesEXT  enable_depth_clip_object = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT,
				.pNext = &enable_shader_object,
				.depthClipEnable = VK_TRUE
			};

			VkDeviceCreateInfo device_create_info = {};
			device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			device_create_info.pNext = &enable_depth_clip_object;
			device_create_info.flags = 0;
			device_create_info.queueCreateInfoCount = 1;
			device_create_info.pQueueCreateInfos = &device_queue_create_info;
			device_create_info.enabledLayerCount = 0;
			device_create_info.ppEnabledLayerNames = nullptr;
			device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
			device_create_info.ppEnabledExtensionNames = device_extensions.data();
			device_create_info.pEnabledFeatures = &required_features;

			VkResult result = vkCreateDevice(physical_device, &device_create_info, nullptr, &device);
			if (result != VK_SUCCESS) return result;

			l->debug("Vulkan device created successfully!");
			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_INSTANCE;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(instance);
				debug_name.pObjectName = "rosy instance";
				if (result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
			}
			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_DEVICE;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(device);
				debug_name.pObjectName = "rosy device";
				if (result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
			}
			graphics_created_bitmask |= graphics_created_bit_device;
			return result;
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

		[[nodiscard]] swap_chain_support_details query_swap_chain_support(const VkPhysicalDevice p_device) const
		{
			swap_chain_support_details details = {};
			if (const VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(p_device, surface, &details.capabilities); res != VK_SUCCESS)
			{
				l->warn(std::format("Failed to get device surface capabilities: {}", static_cast<uint8_t>(res)));
				return details;
			}

			uint32_t format_count;
			vkGetPhysicalDeviceSurfaceFormatsKHR(p_device, surface, &format_count, nullptr);

			if (format_count != 0) {
				details.formats.resize(format_count);
				if (const VkResult res = vkGetPhysicalDeviceSurfaceFormatsKHR(p_device, surface, &format_count, details.formats.data()); res != VK_SUCCESS)
				{
					l->warn(std::format("Failed to get device surface formats: {}", static_cast<uint8_t>(res)));
					return details;
				}
			}

			uint32_t present_mode_count;
			if (const VkResult res = vkGetPhysicalDeviceSurfacePresentModesKHR(p_device, surface, &present_mode_count, nullptr); res != VK_SUCCESS)
			{
				l->warn(std::format("Failed to get device surface formats: {}", static_cast<uint8_t>(res)));
				return details;
			}

			if (present_mode_count != 0) {
				details.present_modes.resize(present_mode_count);
				if (const VkResult res = vkGetPhysicalDeviceSurfacePresentModesKHR(p_device, surface, &present_mode_count, details.present_modes.data()); res != VK_SUCCESS)
				{
					l->warn(std::format("Failed to get device surface present modes: {}", static_cast<uint8_t>(res)));
					return details;
				}
			}

			return details;
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


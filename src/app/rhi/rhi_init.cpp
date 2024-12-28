
#include "rhi.h"
#include "../loader/loader.h"
#include "rhi_utils.h"
#include "../../rhi/descriptor.h"

namespace {
	const char* instance_layers[] = {
		"VK_LAYER_LUNARG_api_dump",
		"VK_LAYER_KHRONOS_validation",
		//"VK_LAYER_LUNARG_monitor",
		//"VK_LAYER_KHRONOS_profiles",
		//"VK_LAYER_LUNARG_crash_diagnostic",
		"VK_LAYER_KHRONOS_shader_object",
		"VK_LAYER_KHRONOS_synchronization2",
	};

	VkPhysicalDeviceFeatures required_features;

	const char* instance_extensions[] = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	};

	const char* device_extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
		VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME,
		//VK_KHR_MULTIVIEW_EXTENSION_NAME,
	};
}

rhi::rhi(rosy_config::config* cfg) : app_cfg{ cfg }, required_features_{ required_features }
{
	memset(&required_features_, 0, sizeof(VkPhysicalDeviceFeatures));
}

rhi::~rhi()
{
	deinit();
}

VkResult rhi::init(SDL_Window* window)
{
	VkResult result = volkInitialize();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed initialize volk! %d\n", result);
		return result;
	}
	result = this->query_instance_layers();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to query instance layers! %d\n", result);
		return result;
	}
	result = this->query_instance_extensions();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to query instance extensions! %d\n", result);
		return result;
	}
	result = this->init_instance();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to create Vulkan instance! %d\n", result);
		return result;
	}
	result = this->create_debug_callback();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to create Vulkan debug callback! %d", result);
		return result;
	}
	result = this->init_surface(window);
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to create surface! %d", result);
		return result;
	}
	result = this->init_physical_device();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to create Vulkan physical device! %d\n", result);
		return result;
	}
	result = this->query_device_layers();
	rosy_utils::debug_print_w(L"Failed to query device layers! %d\n", result);
	if (result != VK_SUCCESS)
	{
		return result;
	}
	result = this->query_device_extensions();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to query device extensions! %d\n", result);
		return result;
	}
	result = this->init_device();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to create Vulkan device! %d\n", result);
		return result;
	}
	this->init_allocator();
	result = this->init_presentation_queue();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to get presentation queue! %d\n", result);
		return result;
	}
	result = this->init_swap_chain(window);
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to init swap chain! %d\n", result);
		return result;
	}
	result = this->init_draw_image();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to init draw image! %d\n", result);
		return result;
	}
	result = this->init_descriptors();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to init draw descriptors! %d\n", result);
		return result;
	}
	result = this->init_command_pool();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to init command pool! %d\n", result);
		return result;
	}
	result = this->init_command_buffers();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to init command buffers! %d\n", result);
		return result;
	}
	result = this->init_sync_objects();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to init sync objects! %d\n", result);
		return result;
	}
	result = this->init_ui(window);
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to init UI! %d\n", result);
		return result;
	}
	result = this->init_commands();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to init commands! %d\n", result);
		return result;
	}
	result = this->init_data();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to init data! %d\n", result);
		return result;
	}
	result = this->init_ktx();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to init ktx! %d\n", result);
		return result;
	}
	return VK_SUCCESS;
}

void rhi::deinit()
{
	if (deinited_) return;
	deinited_ = true;
	{
		// Wait for everything to be done.
		if (opt_device.has_value())
		{
			vkDeviceWaitIdle(opt_device.value());
		}
	}

	// Deinit begin in the reverse order from how it was created.
	deinit_ui();

	if (vdi.has_value())
	{
		ktxVulkanDeviceInfo_Destruct(&vdi.value());
	}
	if (imm_fence_.has_value())
	{
		vkDestroyFence(opt_device.value(), imm_fence_.value(), nullptr);
	}
	if (imm_command_pool_.has_value())
	{
		vkDestroyCommandPool(opt_device.value(), imm_command_pool_.value(), nullptr);
	}
	if (descriptor_sets.has_value())
	{
		descriptor_sets.value()->deinit(opt_device.value());
	}
	for (frame_data fd : frame_datas_)
	{
		const VkDevice device = opt_device.value();
		if (fd.multiview_fence.has_value()) vkDestroyFence(device, fd.multiview_fence.value(), nullptr);
		if (fd.in_flight_fence.has_value()) vkDestroyFence(device, fd.in_flight_fence.value(), nullptr);
		if (fd.image_available_semaphore.has_value())
			vkDestroySemaphore(
				device, fd.image_available_semaphore.value(), nullptr);
		if (fd.multiview_semaphore.has_value())
			vkDestroySemaphore(
				device, fd.multiview_semaphore.value(), nullptr);
		if (fd.render_finished_semaphore.has_value())
			vkDestroySemaphore(
				device, fd.render_finished_semaphore.value(), nullptr);
		if (fd.command_pool.has_value()) vkDestroyCommandPool(device, fd.command_pool.value(), nullptr);
	}

	{
		// Deinit draw images
		if (shadow_map_image_.has_value())
		{
			const allocated_image depth_image = shadow_map_image_.value();
			vkDestroyImageView(opt_device.value(), depth_image.image_view, nullptr);
			vmaDestroyImage(opt_allocator.value(), depth_image.image, depth_image.allocation);
		}
		if (depth_image_.has_value())
		{
			const allocated_image depth_image = depth_image_.value();
			vkDestroyImageView(opt_device.value(), depth_image.image_view, nullptr);
			vmaDestroyImage(opt_allocator.value(), depth_image.image, depth_image.allocation);
		}

		if (draw_image_.has_value())
		{
			const allocated_image draw_image = draw_image_.value();
			vkDestroyImageView(opt_device.value(), draw_image.image_view, nullptr);
			vmaDestroyImage(opt_allocator.value(), draw_image.image, draw_image.allocation);
		}
	}

	destroy_swapchain();

	if (debug_messenger_.has_value())
	{
		vkDestroyDebugUtilsMessengerEXT(instance_.value(), debug_messenger_.value(), nullptr);
	}

	if (opt_allocator.has_value())
	{
		vmaDestroyAllocator(opt_allocator.value());
	}

	if (opt_device.has_value())
	{
		if (const VkResult result = vkDeviceWaitIdle(opt_device.value()); result == VK_SUCCESS)
			vkDestroyDevice(
				opt_device.value(), nullptr);
	}

	if (surface_.has_value())
	{
		SDL_Vulkan_DestroySurface(instance_.value(), surface_.value(), nullptr);
	}

	if (instance_.has_value())
	{
		vkDestroyInstance(instance_.value(), nullptr);
	}
}

void rhi::destroy_swapchain()
{
	vkDeviceWaitIdle(opt_device.value());
	for (const VkImageView image_view : swap_chain_image_views_)
	{
		vkDestroyImageView(opt_device.value(), image_view, nullptr);
	}
	swap_chain_image_views_.clear();
	if (swapchain_.has_value())
	{
		vkDestroySwapchainKHR(opt_device.value(), swapchain_.value(), nullptr);
		swapchain_ = std::nullopt;
	}
}

VkResult rhi::resize_swapchain(SDL_Window* window)
{
	vkDeviceWaitIdle(opt_device.value());
	destroy_swapchain();
	return create_swapchain(window, VK_NULL_HANDLE);
}

VkResult rhi::query_instance_layers()
{
	uint32_t p_property_count = 0;
	VkResult result = vkEnumerateInstanceLayerProperties(&p_property_count, nullptr);
	if (result != VK_SUCCESS) return result;
	rosy_utils::debug_print_a("Found %d instance layers\n", p_property_count);
	if (p_property_count == 0) return result;
	std::vector<VkLayerProperties> layers;
	layers.resize(p_property_count);
	result = vkEnumerateInstanceLayerProperties(&p_property_count, layers.data());
	if (result != VK_SUCCESS) return result;
	if (!app_cfg->enable_validation_layers) return result;
	for (VkLayerProperties lp : layers)
	{
		rosy_utils::debug_print_a("Instance layer name: %s layer description: %s\n", lp.layerName, lp.description);
		for (const char* layerName : instance_layers)
		{
			if (strcmp(layerName, lp.layerName) == 0)
			{
				rosy_utils::debug_print_a("\tAdding instance layer: %s\n", lp.layerName);
				instance_layer_properties_.push_back(layerName);
			}
		}
	}
	return result;
}

VkResult rhi::query_device_layers() const
{
	if (!physical_device_.has_value()) return VK_NOT_READY;
	uint32_t p_property_count = 0;
	VkResult result = vkEnumerateDeviceLayerProperties(physical_device_.value(), &p_property_count, nullptr);
	if (result != VK_SUCCESS) return result;
	rosy_utils::debug_print_a("Found %d device layers\n", p_property_count);
	if (p_property_count == 0) return result;
	std::vector<VkLayerProperties> layers;
	layers.resize(p_property_count);
	result = vkEnumerateDeviceLayerProperties(physical_device_.value(), &p_property_count, layers.data());
	if (result != VK_SUCCESS) return result;
	for (VkLayerProperties lp : layers)
	{
		rosy_utils::debug_print_a("Device layer name: %s layer description: %s\n", lp.layerName, lp.description);
	}
	return result;
}

VkResult rhi::query_instance_extensions()
{
	uint32_t p_property_count = 0;
	VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &p_property_count, nullptr);
	if (result != VK_SUCCESS) return result;

	rosy_utils::debug_print_a("Found %d instance extensions\n", p_property_count);
	if (p_property_count == 0) return result;

	std::vector<VkExtensionProperties> extensions;
	extensions.resize(p_property_count);
	result = vkEnumerateInstanceExtensionProperties(nullptr, &p_property_count, extensions.data());
	if (result != VK_SUCCESS) return result;
	rosy_utils::debug_print_a("num required instance extensions: %d\n", std::size(instance_extensions));
	{
		// Setup required instance extensions
		size_t found_extensions = 0;
		uint32_t extension_count;
		const auto extension_names = SDL_Vulkan_GetInstanceExtensions(&extension_count);
		for (uint32_t i = 0; i < extension_count; i++)
		{
			rosy_utils::debug_print_a("pushing back required SDL instance extension with name: %s\n",
				extension_names[i]);
			instance_extensions_.push_back(extension_names[i]);
		}
		for (uint32_t i = 0; i < std::size(instance_extensions); i++)
		{
			rosy_utils::debug_print_a("pushing back required rosy instance extension with name: %s\n",
				instance_extensions[i]);
			instance_extensions_.push_back(instance_extensions[i]);
		}
	}
	rosy_utils::debug_print_a("num instanceExtensions: %d\n", instance_extensions_.size());

	std::vector<const char*> required_instance_extensions(std::begin(instance_extensions_),
		std::end(instance_extensions_));
	for (auto [extensionName, specVersion] : extensions)
	{
		rosy_utils::debug_print_a("Instance extension name: %s\n", extensionName);
		for (const char* extension_name : instance_extensions_)
		{
			if (strcmp(extension_name, extensionName) == 0)
			{
				rosy_utils::debug_print_a("\tRequiring instance extension: %s\n", extension_name);
				std::erase(required_instance_extensions, extension_name);
			}
		}
	}
	if (required_instance_extensions.size() != 0)
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
	return result;
}

VkResult rhi::query_device_extensions()
{
	uint32_t p_property_count = 0;
	if (!physical_device_.has_value()) return VK_NOT_READY;

	VkResult result = vkEnumerateDeviceExtensionProperties(physical_device_.value(), nullptr, &p_property_count, nullptr);
	if (result != VK_SUCCESS) return result;

	rosy_utils::debug_print_a("Found %d device extensions\n", p_property_count);
	if (p_property_count == 0) return result;

	std::vector<VkExtensionProperties> extensions;
	extensions.resize(p_property_count);

	result = vkEnumerateDeviceExtensionProperties(physical_device_.value(), nullptr, &p_property_count,
		extensions.data());
	if (result != VK_SUCCESS) return result;

	// validate required device extensions
	std::vector<const char*> required_device_extensions(std::begin(device_extensions), std::end(device_extensions));

	for (auto [extensionName, specVersion] : extensions)
	{
		rosy_utils::debug_print_a("Device extension name: %s\n", extensionName);
		for (const char* extension_name : device_extensions)
		{
			if (strcmp(extension_name, extensionName) == 0)
			{
				rosy_utils::debug_print_a("\tRequiring device extension: %s\n", extension_name);
				device_device_extensions_.push_back(extension_name);
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


VkResult rhi::create_debug_callback()
{
	if (!app_cfg->enable_validation_layers) return VK_SUCCESS;

	const VkDebugUtilsMessengerCreateInfoEXT create_info = create_debug_callback_info();
	VkDebugUtilsMessengerEXT debug_messenger;
	const VkResult result = vkCreateDebugUtilsMessengerEXT(instance_.value(), &create_info, nullptr, &debug_messenger);
	if (result != VK_SUCCESS) return result;
	debug_messenger_ = debug_messenger;
	return result;
}

VkResult rhi::init_surface(SDL_Window* window)
{
	VkSurfaceKHR surface;
	SDL_Vulkan_CreateSurface(window, instance_.value(), nullptr, &surface);
	surface_ = surface;
	return VK_SUCCESS;
}

VkResult rhi::init_instance()
{
	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Rosy";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "Rosy";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_3;

	const VkDebugUtilsMessengerCreateInfoEXT create_debug_callback_info_ext = create_debug_callback_info();
	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pNext = &create_debug_callback_info_ext;
	create_info.pApplicationInfo = &app_info;
	create_info.enabledLayerCount = instance_layer_properties_.size();
	create_info.ppEnabledLayerNames = instance_layer_properties_.data();
	create_info.enabledExtensionCount = instance_extensions_.size();
	create_info.ppEnabledExtensionNames = instance_extensions_.data();

	VkInstance instance;
	const VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
	if (result != VK_SUCCESS) return result;
	OutputDebugStringW(L"Vulkan instance created successfully!\n");
	volkLoadInstance(instance);
	instance_ = instance;
	return result;
}

VkResult rhi::init_physical_device()
{
	if (!instance_.has_value()) return VK_NOT_READY;
	std::vector<VkPhysicalDevice> physical_devices;

	uint32_t physical_device_count = 0;
	VkResult result = vkEnumeratePhysicalDevices(instance_.value(), &physical_device_count, nullptr);

	physical_devices.resize(physical_device_count);
	vkEnumeratePhysicalDevices(instance_.value(), &physical_device_count, &physical_devices[0]);
	bool found_device = false;
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


		if (device_properties.vendorID == app_cfg->device_vendor)
		{
			{
				found_device = true;
				physical_device_ = p_device;
				physical_device_properties_ = device_properties;
			}
			{
				// features
				VkPhysicalDeviceFeatures features;
				vkGetPhysicalDeviceFeatures(p_device, &features);
				supported_features_ = features;
			}
			{
				// memory
				VkPhysicalDeviceMemoryProperties mem_props;
				vkGetPhysicalDeviceMemoryProperties(p_device, &mem_props);
				physical_device_memory_properties_ = mem_props;
			}
			{
				// queues
				uint32_t queue_count = 0;
				vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queue_count, nullptr);
				std::vector<VkQueueFamilyProperties> queueFamilyPropertiesData;
				queueFamilyPropertiesData.resize(queue_count);
				vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queue_count, &queueFamilyPropertiesData[0]);
				queue_family_properties_ = queueFamilyPropertiesData;
			}
		}
	}
	if (!found_device) return VK_ERROR_FEATURE_NOT_PRESENT;
	uint32_t queue_count = 0;
	uint32_t queue_index = 0;
	required_features.multiDrawIndirect = VK_TRUE;
	required_features.tessellationShader = VK_TRUE;
	required_features.geometryShader = VK_TRUE;
	required_features.fillModeNonSolid = VK_TRUE;
	required_features.wideLines = VK_TRUE;
	required_features.shaderInt64 = VK_TRUE;
	required_features_ = required_features;
	if (!physical_device_.has_value()) return VK_NOT_READY;

	VkPhysicalDevice p_device = physical_device_.value();
	std::vector<VkQueueFamilyProperties> queue_family_properties_data = queue_family_properties_.value();
	bool found_queue = false;
	for (std::uint32_t i = 0; i < queue_family_properties_data.size(); ++i)
	{
		VkQueueFamilyProperties q_props = queue_family_properties_data[i];
		if (q_props.timestampValidBits < 64) continue;
		if (!(q_props.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT |
			VK_QUEUE_SPARSE_BINDING_BIT)))
			continue;
		VkBool32 present_support = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(p_device, i, surface_.value(), &present_support);
		if (!present_support) continue;
		if (q_props.queueCount > queue_count)
		{
			found_queue = true;
			queue_index = i;
			queue_count = q_props.queueCount;
		}
	}
	if (!found_queue)
	{
		rosy_utils::debug_print_a("No suitable queue found!");
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}
	queue_index_ = queue_index;
	queue_count_ = queue_count;
	OutputDebugStringW(L"Vulkan physical device created successfully!\n");
	return result;
}

VkResult rhi::init_device()
{
	if (!physical_device_.has_value()) return VK_NOT_READY;

	VkDeviceQueueCreateInfo device_queue_create_info{};
	device_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	device_queue_create_info.pNext = nullptr;
	device_queue_create_info.flags = 0;
	device_queue_create_info.queueFamilyIndex = queue_index_;
	queue_priorities_.resize(queue_count_, 0.5f);
	device_queue_create_info.pQueuePriorities = queue_priorities_.data();
	device_queue_create_info.queueCount = queue_count_;
	VkDeviceCreateInfo device_create_info = {};

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


	VkPhysicalDeviceVulkan11Features vulkan11_features{};
	vulkan11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	vulkan11_features.pNext = &vulkan12_features;
	vulkan11_features.variablePointers = VK_TRUE;
	vulkan11_features.variablePointersStorageBuffer = VK_TRUE;
	vulkan11_features.multiview = VK_TRUE;

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

	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_create_info.pNext = &enable_depth_clip_object;
	device_create_info.flags = 0;
	device_create_info.queueCreateInfoCount = 1;
	device_create_info.pQueueCreateInfos = &device_queue_create_info;
	device_create_info.enabledLayerCount = 0;
	device_create_info.ppEnabledLayerNames = nullptr;
	device_create_info.enabledExtensionCount = device_device_extensions_.size();
	device_create_info.ppEnabledExtensionNames = device_device_extensions_.data();
	device_create_info.pEnabledFeatures = &required_features_;
	VkDevice device;
	VkResult result = vkCreateDevice(physical_device_.value(), &device_create_info, nullptr, &device);
	if (result != VK_SUCCESS) return result;

	rosy_utils::debug_print_w(L"Vulkan device created successfully!\n");
	opt_device = device;
	return result;
}

VkResult rhi::init_presentation_queue()
{
	VkQueue queue;
	VkDeviceQueueInfo2 get_info{};
	get_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	get_info.flags = 0;
	get_info.queueFamilyIndex = queue_index_;
	get_info.queueIndex = 0;
	vkGetDeviceQueue2(opt_device.value(), &get_info, &queue);
	present_queue_ = queue;
	return VK_SUCCESS;
}

namespace
{
	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats)
	{
		for (const auto& available_format : available_formats)
		{
			if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace ==
				VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return available_format;
			}
		}
		return available_formats[0];
	}

	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes)
	{
		for (const auto& available_present_mode : available_present_modes)
		{
			if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return available_present_mode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window)
	{
		if (constexpr uint32_t max_u32 = (std::numeric_limits<uint32_t>::max)(); capabilities.currentExtent.width !=
			max_u32)
		{
			return capabilities.currentExtent;
		}
		int width, height;
		SDL_GetWindowSizeInPixels(window, &width, &height);

		VkExtent2D actual_extent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width,
			capabilities.maxImageExtent.width);
		actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height,
			capabilities.maxImageExtent.height);

		return actual_extent;
	}
}

VkResult rhi::init_swap_chain(SDL_Window* window)
{
	return create_swapchain(window, VK_NULL_HANDLE);
}

VkResult rhi::create_swapchain(SDL_Window* window, const VkSwapchainKHR old_swapchain)
{
	swapchain_details_ = query_swap_chain_support(physical_device_.value());

	swapchain_image_format_ = choose_swap_surface_format(swapchain_details_.formats);
	swapchain_present_mode_ = choose_swap_present_mode(swapchain_details_.present_modes);

	swap_chain_image_count_ = swapchain_details_.capabilities.minImageCount;
	if (swapchain_details_.capabilities.maxImageCount > 0 && swap_chain_image_count_ > swapchain_details_.capabilities.
		maxImageCount)
	{
		swap_chain_image_count_ = swapchain_details_.capabilities.maxImageCount;
	}
	frame_datas_.resize(std::min(static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT), swap_chain_image_count_));

	const VkDevice device = opt_device.value();
	VkSwapchainKHR swapchain{};
	const VkExtent2D extent = choose_swap_extent(swapchain_details_.capabilities, window);
	{
		VkSwapchainCreateInfoKHR create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		create_info.surface = surface_.value();
		create_info.minImageCount = swap_chain_image_count_;
		create_info.imageFormat = swapchain_image_format_.format;
		create_info.imageColorSpace = swapchain_image_format_.colorSpace;
		create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		create_info.imageExtent = extent;
		create_info.imageArrayLayers = 1;
		create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		// Just one queue family right now.
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.queueFamilyIndexCount = 0;
		create_info.pQueueFamilyIndices = nullptr;

		create_info.preTransform = swapchain_details_.capabilities.currentTransform;

		create_info.presentMode = swapchain_present_mode_;
		create_info.clipped = VK_TRUE;

		create_info.oldSwapchain = old_swapchain;

		if (const VkResult result = vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain); result !=
			VK_SUCCESS)
			return result;
	}

	if (old_swapchain != VK_NULL_HANDLE)
	{
		destroy_swapchain();
	}
	{
		swapchain_extent = extent;
		swapchain_ = swapchain;

		swap_chain_images_.clear();
		vkGetSwapchainImagesKHR(device, swapchain, &swap_chain_image_count_, nullptr);
		swap_chain_images_.resize(swap_chain_image_count_);
		vkGetSwapchainImagesKHR(device, swapchain, &swap_chain_image_count_, swap_chain_images_.data());

		for (size_t i = 0; i < swap_chain_images_.size(); i++)
		{
			VkImageViewCreateInfo create_info = rhi_helpers::img_view_create_info(swapchain_image_format_.format,
				swap_chain_images_[i], VK_IMAGE_ASPECT_COLOR_BIT);
			VkImageView image_view{};
			if (const VkResult result = vkCreateImageView(device, &create_info, nullptr, &image_view); result !=
				VK_SUCCESS)
				return result;
			// don't initially size these so we can clean this up nicely if any fail
			swap_chain_image_views_.push_back(image_view);
		}
	}
	return VK_SUCCESS;
}

VkResult rhi::init_draw_image()
{
	VkResult result;
	const VkDevice device = opt_device.value();
	VkExtent3D draw_image_extent = {
		.width = static_cast<uint32_t>(app_cfg->max_window_width),
		.height = static_cast<uint32_t>(app_cfg->max_window_height),
		.depth = 1
	};

	VmaAllocationCreateInfo r_img_alloc_info{};
	r_img_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	r_img_alloc_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	{
		// Draw image creation.
		allocated_image draw_image{};
		draw_image.image_format = VK_FORMAT_R16G16B16A16_SFLOAT;
		draw_image.image_extent = draw_image_extent;

		VkImageUsageFlags draw_image_usages{};
		draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
		draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		VkImageCreateInfo draw_info = rhi_helpers::img_create_info(draw_image.image_format, draw_image_usages, draw_image_extent);

		vmaCreateImage(opt_allocator.value(), &draw_info, &r_img_alloc_info, &draw_image.image, &draw_image.allocation,
			nullptr);

		VkImageViewCreateInfo r_view_info = rhi_helpers::img_view_create_info(draw_image.image_format, draw_image.image,
			VK_IMAGE_ASPECT_COLOR_BIT);

		result = vkCreateImageView(device, &r_view_info, nullptr, &draw_image.image_view);
		if (result != VK_SUCCESS) return result;
		draw_image_ = draw_image;
	}
	{
		// Depth image creation.
		allocated_image depth_image{};
		depth_image.image_format = VK_FORMAT_D32_SFLOAT;
		depth_image.image_extent = draw_image_extent;
		VkImageUsageFlags depth_image_usages{};
		depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		VkImageCreateInfo depth_info = rhi_helpers::img_create_info(depth_image.image_format, depth_image_usages, draw_image_extent);

		vmaCreateImage(opt_allocator.value(), &depth_info, &r_img_alloc_info, &depth_image.image, &depth_image.allocation,
			nullptr);

		VkImageViewCreateInfo d_view_info = rhi_helpers::img_view_create_info(depth_image.image_format, depth_image.image,
			VK_IMAGE_ASPECT_DEPTH_BIT);

		result = vkCreateImageView(device, &d_view_info, nullptr, &depth_image.image_view);
		if (result != VK_SUCCESS) return result;
		depth_image_ = depth_image;
	}
	{
		// Shadow map image creation.
		VkExtent3D shadow_map_image_extent = {
			.width = 1024,
			.height = 1024,
			.depth = 1
		};
		allocated_image shadow_map_image{};
		shadow_map_image.image_format = VK_FORMAT_D32_SFLOAT;
		shadow_map_image.image_extent = shadow_map_image_extent;
		VkImageUsageFlags depth_image_usages{};

		depth_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		VkImageCreateInfo depth_info = rhi_helpers::shadow_img_create_info(shadow_map_image.image_format, depth_image_usages, shadow_map_image_extent);

		vmaCreateImage(opt_allocator.value(), &depth_info, &r_img_alloc_info, &shadow_map_image.image, &shadow_map_image.allocation,
			nullptr);

		VkImageViewCreateInfo d_view_info = rhi_helpers::shadow_img_view_create_info(shadow_map_image.image_format, shadow_map_image.image,
			VK_IMAGE_ASPECT_DEPTH_BIT);

		result = vkCreateImageView(device, &d_view_info, nullptr, &shadow_map_image.image_view);
		if (result != VK_SUCCESS) return result;
		shadow_map_image_ = shadow_map_image;
		auto obj_name = "shadow_map_image";
		const VkDebugUtilsObjectNameInfoEXT vert_name = rhi_helpers::add_name(VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(shadow_map_image.image), obj_name);
		if (const VkResult debug_result = vkSetDebugUtilsObjectNameEXT(device, &vert_name); debug_result != VK_SUCCESS) return debug_result;
	}

	return result;
}

VkResult rhi::init_descriptors()
{
	auto dsm = std::make_unique<descriptor_sets_manager>(descriptor_sets_manager{});
	if (const VkResult result = dsm->init_pool(opt_device.value()); result != VK_SUCCESS) return result;
	descriptor_sets = std::move(dsm);
	return VK_SUCCESS;
}

void rhi::init_allocator()
{
	constexpr VkDeviceSize device_size = 0;
	VmaVulkanFunctions vulkan_functions{};
	vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocator_create_info{};
	allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_3;
	allocator_create_info.physicalDevice = physical_device_.value();
	allocator_create_info.device = opt_device.value();
	allocator_create_info.instance = instance_.value();
	allocator_create_info.pVulkanFunctions = &vulkan_functions;
	allocator_create_info.preferredLargeHeapBlockSize = device_size;
	allocator_create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	VmaAllocator allocator;
	vmaCreateAllocator(&allocator_create_info, &allocator);
	opt_allocator = allocator;
}

VkResult rhi::init_command_pool()
{
	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = queue_index_;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkCommandPool command_pool{};
		if (const VkResult result = vkCreateCommandPool(opt_device.value(), &pool_info, nullptr, &command_pool); result !=
			VK_SUCCESS)
			return result;
		frame_datas_[i].command_pool = command_pool;
	}
	return VK_SUCCESS;
}

VkResult rhi::init_command_buffers()
{
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		{
			// multi view command buffer
			// render command buffer
			VkCommandBufferAllocateInfo alloc_info{};
			alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			alloc_info.commandPool = frame_datas_[i].command_pool.value();
			alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			alloc_info.commandBufferCount = 1;

			VkCommandBuffer command_buffer{};
			if (const VkResult result = vkAllocateCommandBuffers(opt_device.value(), &alloc_info, &command_buffer); result !=
				VK_SUCCESS)
				return result;
			frame_datas_[i].multiview_command_buffer = command_buffer;
		}

		{
			// render command buffer
			VkCommandBufferAllocateInfo alloc_info{};
			alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			alloc_info.commandPool = frame_datas_[i].command_pool.value();
			alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			alloc_info.commandBufferCount = 1;

			VkCommandBuffer command_buffer{};
			if (const VkResult result = vkAllocateCommandBuffers(opt_device.value(), &alloc_info, &command_buffer); result !=
				VK_SUCCESS)
				return result;
			frame_datas_[i].render_command_buffer = command_buffer;
		}
	}
	return VK_SUCCESS;
}

VkResult rhi::init_sync_objects()
{
	VkSemaphoreCreateInfo semaphore_info{};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fence_info{};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkResult result;
	const VkDevice device = opt_device.value();
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		{
			VkSemaphore semaphore;
			result = vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphore);
			if (result != VK_SUCCESS) return result;
			frame_datas_[i].image_available_semaphore = semaphore;
		}
		{
			VkSemaphore semaphore;
			result = vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphore);
			if (result != VK_SUCCESS) return result;
			frame_datas_[i].render_finished_semaphore = semaphore;
		}
		{
			VkSemaphore semaphore;
			result = vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphore);
			if (result != VK_SUCCESS) return result;
			frame_datas_[i].multiview_semaphore = semaphore;
		}
		{
			VkFence fence;
			result = vkCreateFence(device, &fence_info, nullptr, &fence);
			if (result != VK_SUCCESS) return result;
			frame_datas_[i].multiview_fence = fence;
		}
		{
			VkFence fence;
			result = vkCreateFence(device, &fence_info, nullptr, &fence);
			if (result != VK_SUCCESS) return result;
			frame_datas_[i].in_flight_fence = fence;
		}
	}
	{
		VkFence fence;
		result = vkCreateFence(device, &fence_info, nullptr, &fence);
		if (result != VK_SUCCESS) return result;
		imm_fence_ = fence;
	}
	return VK_SUCCESS;
}


VkResult rhi::init_commands()
{
	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = queue_index_;

	const VkDevice device = opt_device.value();
	VkCommandPool command_pool;
	VkResult result = vkCreateCommandPool(device, &pool_info, nullptr, &command_pool);
	if (result != VK_SUCCESS) return result;
	imm_command_pool_ = command_pool;

	// allocate the command data for immediate submits
	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = imm_command_pool_.value();
	alloc_info.commandBufferCount = 1;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VkCommandBuffer buffer;
	result = vkAllocateCommandBuffers(device, &alloc_info, &buffer);
	if (result != VK_SUCCESS) return result;
	imm_command_buffer_ = buffer;

	return VK_SUCCESS;
}

VkResult rhi::init_data()
{
	buffer = std::make_unique<rhi_data>(rhi_data{this});
	return VK_SUCCESS;
}

VkResult rhi::init_ktx()
{
	const vulkan_ctx vk_ctx = {
		.gpu = physical_device_.value(),
		.device = opt_device.value(),
		.queue = present_queue_.value(),
		.cmd_pool = imm_command_pool_.value(),
	};
	ktxVulkanDeviceInfo new_vdi;
	ktxVulkanDeviceInfo_Construct(&new_vdi, vk_ctx.gpu, vk_ctx.device,
		vk_ctx.queue, vk_ctx.cmd_pool, nullptr);
	vdi = new_vdi;
	return VK_SUCCESS;
}
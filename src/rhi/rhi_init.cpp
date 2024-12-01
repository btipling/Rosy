
#include "rhi.h"
#include "../loader/loader.h"
#include "rhi_utils.h"

static const char* instanceLayers[] = {
	"VK_LAYER_LUNARG_api_dump",
	"VK_LAYER_KHRONOS_validation",
	//"VK_LAYER_LUNARG_monitor",
	//"VK_LAYER_KHRONOS_profiles",
	//"VK_LAYER_LUNARG_crash_diagnostic",
	"VK_LAYER_KHRONOS_shader_object",
	"VK_LAYER_KHRONOS_synchronization2",
};
static VkPhysicalDeviceFeatures requiredFeatures;

static const char* instanceExtensions[] = {
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
};

static const char* deviceExtensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
	//VK_KHR_MULTIVIEW_EXTENSION_NAME,
};

rhi::rhi(rosy_config::Config cfg) : cfg_{ cfg }, required_features_{ requiredFeatures }
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
	result = this->init_graphics();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to init graphics! %d\n", result);
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
	result = this->init_buffer();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to init buffer! %d\n", result);
		return result;
	}
	result = this->init_default_data();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to init default data! %d\n", result);
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
		if (device_.has_value())
		{
			vkDeviceWaitIdle(device_.value());
		}
	}

	// Deinit begin in the reverse order from how it was created.
	deinit_ui();
	{
		if (default_sampler_nearest_.has_value()) vkDestroySampler(device_.value(), default_sampler_nearest_.value(), nullptr);
		if (default_sampler_linear_.has_value()) vkDestroySampler(device_.value(), default_sampler_linear_.value(), nullptr);

		if (white_image_.has_value()) destroy_image(white_image_.value());
		if (grey_image_.has_value()) destroy_image(grey_image_.value());
		if (black_image_.has_value()) destroy_image(black_image_.value());
		if (error_checkerboard_image_.has_value()) destroy_image(error_checkerboard_image_.value());
	}

	for (std::shared_ptr<mesh_asset> mesh : test_meshes_)
	{
		gpu_mesh_buffers rectangle = mesh.get()->mesh_buffers;
		destroy_buffer(rectangle.vertex_buffer);
		destroy_buffer(rectangle.index_buffer);
		mesh.reset();
	}

	if (imm_fence_.has_value())
	{
		vkDestroyFence(device_.value(), imm_fence_.value(), nullptr);
	}

	if (imm_command_pool_.has_value())
	{
		vkDestroyCommandPool(device_.value(), imm_command_pool_.value(), nullptr);
	}

	for (frame_data fd : frame_datas_)
	{
		VkDevice device = device_.value();
		if (fd.in_flight_fence.has_value()) vkDestroyFence(device, fd.in_flight_fence.value(), nullptr);
		if (fd.image_available_semaphore.has_value())
			vkDestroySemaphore(
				device, fd.image_available_semaphore.value(), nullptr);
		if (fd.render_finished_semaphore.has_value())
			vkDestroySemaphore(
				device, fd.render_finished_semaphore.value(), nullptr);
		if (fd.command_pool.has_value()) vkDestroyCommandPool(device, fd.command_pool.value(), nullptr);
		if (fd.frame_descriptors.has_value()) {
			rosy_utils::debug_print_a("destroying pools?\n");
			fd.frame_descriptors.value().destroy_pools(device);
		}
		if (fd.gpu_scene_buffer.has_value()) destroy_buffer(fd.gpu_scene_buffer.value());
	}
	if (test_mesh_pipeline_.has_value()) {
		test_mesh_pipeline_.value().deinit(device_.value());
	}
	if (single_image_descriptor_layout_.has_value())
	{
		vkDestroyDescriptorSetLayout(device_.value(), single_image_descriptor_layout_.value(), nullptr);
	}
	if (gpu_scene_data_descriptor_layout_.has_value())
	{
		vkDestroyDescriptorSetLayout(device_.value(), gpu_scene_data_descriptor_layout_.value(), nullptr);
	}
	if (draw_image_descriptor_layout_.has_value())
	{
		vkDestroyDescriptorSetLayout(device_.value(), draw_image_descriptor_layout_.value(), nullptr);
	}
	if (global_descriptor_allocator_.has_value())
	{
		global_descriptor_allocator_.value().destroy_pool(device_.value());
	}
	if (depth_image_.has_value())
	{
		const allocated_image depth_image = depth_image_.value();
		vkDestroyImageView(device_.value(), depth_image.image_view, nullptr);
		vmaDestroyImage(allocator_.value(), depth_image.image, depth_image.allocation);
	}

	if (draw_image_.has_value())
	{
		const allocated_image draw_image = draw_image_.value();
		vkDestroyImageView(device_.value(), draw_image.image_view, nullptr);
		vmaDestroyImage(allocator_.value(), draw_image.image, draw_image.allocation);
	}

	destroy_swapchain();

	if (debug_messenger_.has_value())
	{
		vkDestroyDebugUtilsMessengerEXT(instance_.value(), debug_messenger_.value(), nullptr);
	}

	if (allocator_.has_value())
	{
		vmaDestroyAllocator(allocator_.value());
	}

	if (device_.has_value())
	{
		if (const VkResult result = vkDeviceWaitIdle(device_.value()); result == VK_SUCCESS)
			vkDestroyDevice(
				device_.value(), nullptr);
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
	vkDeviceWaitIdle(device_.value());
	for (VkImageView imageView : swap_chain_image_views_)
	{
		vkDestroyImageView(device_.value(), imageView, nullptr);
	}
	swap_chain_image_views_.clear();
	if (swapchain_.has_value())
	{
		vkDestroySwapchainKHR(device_.value(), swapchain_.value(), nullptr);
		swapchain_ = std::nullopt;
	}
}

VkResult rhi::resize_swapchain(SDL_Window* window)
{
	vkDeviceWaitIdle(device_.value());
	destroy_swapchain();
	return create_swapchain(window, VK_NULL_HANDLE);
}

VkResult rhi::draw_frame()
{
	VkResult result = this->render_frame();
	if (result != VK_SUCCESS)
	{
		rosy_utils::debug_print_w(L"Failed to record command buffer! %d\n", result);
		return result;
	}
	return VK_SUCCESS;
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
	if (!cfg_.enable_validation_layers) return result;
	for (VkLayerProperties lp : layers)
	{
		rosy_utils::debug_print_a("Instance layer name: %s layer description: %s\n", lp.layerName, lp.description);
		for (const char* layerName : instanceLayers)
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
	uint32_t pPropertyCount = 0;
	VkResult result = vkEnumerateDeviceLayerProperties(physical_device_.value(), &pPropertyCount, nullptr);
	if (result != VK_SUCCESS) return result;
	rosy_utils::debug_print_a("Found %d device layers\n", pPropertyCount);
	if (pPropertyCount == 0) return result;
	std::vector<VkLayerProperties> layers;
	layers.resize(pPropertyCount);
	result = vkEnumerateDeviceLayerProperties(physical_device_.value(), &pPropertyCount, layers.data());
	if (result != VK_SUCCESS) return result;
	for (VkLayerProperties lp : layers)
	{
		rosy_utils::debug_print_a("Device layer name: %s layer description: %s\n", lp.layerName, lp.description);
	}
	return result;
}

VkResult rhi::query_instance_extensions()
{
	uint32_t pPropertyCount = 0;
	VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &pPropertyCount, nullptr);
	if (result != VK_SUCCESS) return result;

	rosy_utils::debug_print_a("Found %d instance extensions\n", pPropertyCount);
	if (pPropertyCount == 0) return result;

	std::vector<VkExtensionProperties> extensions;
	extensions.resize(pPropertyCount);
	result = vkEnumerateInstanceExtensionProperties(nullptr, &pPropertyCount, extensions.data());
	if (result != VK_SUCCESS) return result;
	rosy_utils::debug_print_a("num required instance extensions: %d\n", std::size(instanceExtensions));
	{
		// Setup required instance extensions
		size_t found_extensions = 0;
		uint32_t extensionCount;
		auto extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
		for (uint32_t i = 0; i < extensionCount; i++)
		{
			rosy_utils::debug_print_a("pushing back required SDL instance extension with name: %s\n",
				extensionNames[i]);
			instance_extensions_.push_back(extensionNames[i]);
		}
		for (uint32_t i = 0; i < std::size(instanceExtensions); i++)
		{
			rosy_utils::debug_print_a("pushing back required rosy instance extension with name: %s\n",
				instanceExtensions[i]);
			instance_extensions_.push_back(instanceExtensions[i]);
		}
	}
	rosy_utils::debug_print_a("num instanceExtensions: %d\n", instance_extensions_.size());

	std::vector<const char*> requiredInstanceExtensions(std::begin(instance_extensions_),
		std::end(instance_extensions_));
	for (VkExtensionProperties ep : extensions)
	{
		rosy_utils::debug_print_a("Instance extension name: %s\n", ep.extensionName);
		for (const char* extensionName : instance_extensions_)
		{
			if (strcmp(extensionName, ep.extensionName) == 0)
			{
				rosy_utils::debug_print_a("\tRequiring instance extension: %s\n", extensionName);
				std::erase(requiredInstanceExtensions, extensionName);
			}
		}
	}
	if (requiredInstanceExtensions.size() != 0)
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
	return result;
}

VkResult rhi::query_device_extensions()
{
	uint32_t pPropertyCount = 0;
	if (!physical_device_.has_value()) return VK_NOT_READY;

	VkResult result = vkEnumerateDeviceExtensionProperties(physical_device_.value(), nullptr, &pPropertyCount, nullptr);
	if (result != VK_SUCCESS) return result;

	rosy_utils::debug_print_a("Found %d device extensions\n", pPropertyCount);
	if (pPropertyCount == 0) return result;

	std::vector<VkExtensionProperties> extensions;
	extensions.resize(pPropertyCount);

	result = vkEnumerateDeviceExtensionProperties(physical_device_.value(), nullptr, &pPropertyCount,
		extensions.data());
	if (result != VK_SUCCESS) return result;

	// validate required device extensions
	std::vector<const char*> requiredDeviceExtensions(std::begin(deviceExtensions), std::end(deviceExtensions));

	for (VkExtensionProperties ep : extensions)
	{
		rosy_utils::debug_print_a("Device extension name: %s\n", ep.extensionName);
		for (const char* extensionName : deviceExtensions)
		{
			if (strcmp(extensionName, ep.extensionName) == 0)
			{
				rosy_utils::debug_print_a("\tRequiring device extension: %s\n", extensionName);
				device_device_extensions_.push_back(extensionName);
				std::erase(requiredDeviceExtensions, extensionName);
			}
		}
	}

	if (requiredDeviceExtensions.size() != 0)
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
	return result;
}


VkResult rhi::create_debug_callback()
{
	if (!cfg_.enable_validation_layers) return VK_SUCCESS;

	VkDebugUtilsMessengerCreateInfoEXT createInfo = create_debug_callback_info();
	VkDebugUtilsMessengerEXT debugMessenger;
	VkResult result = vkCreateDebugUtilsMessengerEXT(instance_.value(), &createInfo, nullptr, &debugMessenger);
	if (result != VK_SUCCESS) return result;
	debug_messenger_ = debugMessenger;
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
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Rosy";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Rosy";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkDebugUtilsMessengerCreateInfoEXT createDebugCallackInfo = create_debug_callback_info();
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext = &createDebugCallackInfo;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledLayerCount = instance_layer_properties_.size();
	createInfo.ppEnabledLayerNames = instance_layer_properties_.data();
	createInfo.enabledExtensionCount = instance_extensions_.size();
	createInfo.ppEnabledExtensionNames = instance_extensions_.data();

	VkInstance instance;
	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
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
		VkPhysicalDeviceShaderObjectFeaturesEXT shader_object_features = {};
		shader_object_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
		shader_object_features.pNext = nullptr;

		VkPhysicalDeviceFeatures2 device_features2 = {};
		device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		device_features2.pNext = &shader_object_features;
		vkGetPhysicalDeviceFeatures2(p_device, &device_features2);

		if (!shader_object_features.shaderObject) continue;


		// dynamic rendering required
		VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features = {};
		buffer_device_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
		buffer_device_address_features.pNext = nullptr;

		device_features2 = {};
		device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		device_features2.pNext = &buffer_device_address_features;
		vkGetPhysicalDeviceFeatures2(p_device, &device_features2);

		if (!buffer_device_address_features.bufferDeviceAddress) continue;


		// buffer device address required
		VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features = {};
		dynamic_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
		dynamic_rendering_features.pNext = nullptr;

		device_features2 = {};
		device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		device_features2.pNext = &dynamic_rendering_features;
		vkGetPhysicalDeviceFeatures2(p_device, &device_features2);

		if (!dynamic_rendering_features.dynamicRendering) continue;


		if (device_properties.vendorID == cfg_.device_vendor)
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
	supported_features_.value_or(requiredFeatures);
	requiredFeatures.multiDrawIndirect = VK_TRUE;
	requiredFeatures.tessellationShader = VK_TRUE;
	requiredFeatures.geometryShader = VK_TRUE;
	requiredFeatures.fillModeNonSolid = VK_TRUE;
	required_features_ = requiredFeatures;
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

	VkDeviceQueueCreateInfo device_queue_create_info = {};
	device_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	device_queue_create_info.pNext = nullptr;
	device_queue_create_info.flags = 0;
	device_queue_create_info.queueFamilyIndex = queue_index_;
	queue_priorities_.resize(queue_count_, 0.5f);
	device_queue_create_info.pQueuePriorities = queue_priorities_.data();
	device_queue_create_info.queueCount = queue_count_;
	VkDeviceCreateInfo device_create_info = {};

	VkPhysicalDeviceVulkan13Features vulkan13_features = {};
	vulkan13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	vulkan13_features.pNext = nullptr;
	vulkan13_features.dynamicRendering = true;
	vulkan13_features.synchronization2 = true;

	VkPhysicalDeviceVulkan12Features vulkan12_features = {};
	vulkan12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	vulkan12_features.pNext = &vulkan13_features;
	vulkan12_features.bufferDeviceAddress = true;
	vulkan12_features.descriptorIndexing = true;
	vulkan12_features.shaderInputAttachmentArrayDynamicIndexing = true;
	vulkan12_features.shaderUniformTexelBufferArrayDynamicIndexing = true;
	vulkan12_features.shaderStorageTexelBufferArrayDynamicIndexing = true;
	vulkan12_features.shaderUniformBufferArrayNonUniformIndexing = true;
	vulkan12_features.shaderSampledImageArrayNonUniformIndexing = true;
	vulkan12_features.shaderStorageBufferArrayNonUniformIndexing = true;
	vulkan12_features.shaderStorageImageArrayNonUniformIndexing = true;
	vulkan12_features.shaderUniformTexelBufferArrayNonUniformIndexing = true;
	vulkan12_features.shaderStorageTexelBufferArrayNonUniformIndexing = true;
	vulkan12_features.descriptorBindingSampledImageUpdateAfterBind = true;
	vulkan12_features.descriptorBindingStorageImageUpdateAfterBind = true;
	vulkan12_features.descriptorBindingStorageBufferUpdateAfterBind = true;
	vulkan12_features.descriptorBindingUniformTexelBufferUpdateAfterBind = true;
	vulkan12_features.descriptorBindingUpdateUnusedWhilePending = true;
	vulkan12_features.descriptorBindingPartiallyBound = true;
	vulkan12_features.descriptorBindingVariableDescriptorCount = true;
	vulkan12_features.runtimeDescriptorArray = true;
	vulkan12_features.samplerFilterMinmax = true;
	vulkan12_features.scalarBlockLayout = true;
	vulkan12_features.imagelessFramebuffer = true;
	vulkan12_features.uniformBufferStandardLayout = true;
	vulkan12_features.shaderSubgroupExtendedTypes = true;

	VkPhysicalDeviceShaderObjectFeaturesEXT enable_shader_object = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,
		.pNext = &vulkan12_features,
		.shaderObject = VK_TRUE
	};

	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_create_info.pNext = &enable_shader_object;
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
	device_ = device;
	return result;
}

VkResult rhi::init_presentation_queue()
{
	VkQueue queue;
	VkDeviceQueueInfo2 get_info = {};
	get_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	get_info.flags = 0;
	get_info.queueFamilyIndex = queue_index_;
	get_info.queueIndex = 0;
	vkGetDeviceQueue2(device_.value(), &get_info, &queue);
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

	const VkDevice device = device_.value();
	VkSwapchainKHR swapchain;
	const VkExtent2D extent = choose_swap_extent(swapchain_details_.capabilities, window);
	{
		VkSwapchainCreateInfoKHR create_info = {};
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
		swapchain_extent_ = extent;
		swapchain_ = swapchain;

		swap_chain_images_.clear();
		vkGetSwapchainImagesKHR(device, swapchain, &swap_chain_image_count_, nullptr);
		swap_chain_images_.resize(swap_chain_image_count_);
		vkGetSwapchainImagesKHR(device, swapchain, &swap_chain_image_count_, swap_chain_images_.data());

		for (size_t i = 0; i < swap_chain_images_.size(); i++)
		{
			VkImageViewCreateInfo create_info = rhi_helpers::img_view_create_info(swapchain_image_format_.format,
				swap_chain_images_[i], VK_IMAGE_ASPECT_COLOR_BIT);
			VkImageView image_view;
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

	VkExtent3D draw_image_extent = {
		.width = static_cast<uint32_t>(cfg_.maxWindowWidth),
		.height = static_cast<uint32_t>(cfg_.maxWindowHeight),
		.depth = 1
	};
	allocated_image draw_image = {};
	draw_image.image_format = VK_FORMAT_R16G16B16A16_SFLOAT;
	draw_image.image_extent = draw_image_extent;

	VkImageUsageFlags draw_image_usages{};
	draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo draw_info = rhi_helpers::img_create_info(draw_image.image_format, draw_image_usages, draw_image_extent);

	VmaAllocationCreateInfo r_img_alloc_info = {};
	r_img_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	r_img_alloc_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(allocator_.value(), &draw_info, &r_img_alloc_info, &draw_image.image, &draw_image.allocation,
		nullptr);

	VkImageViewCreateInfo r_view_info = rhi_helpers::img_view_create_info(draw_image.image_format, draw_image.image,
		VK_IMAGE_ASPECT_COLOR_BIT);

	result = vkCreateImageView(device_.value(), &r_view_info, nullptr, &draw_image.image_view);
	if (result != VK_SUCCESS) return result;
	draw_image_ = draw_image;

	allocated_image depth_image = {};
	depth_image.image_format = VK_FORMAT_D32_SFLOAT;
	depth_image.image_extent = draw_image_extent;
	VkImageUsageFlags depth_image_usages{};
	depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo depth_info = rhi_helpers::img_create_info(depth_image.image_format, depth_image_usages, draw_image_extent);

	vmaCreateImage(allocator_.value(), &depth_info, &r_img_alloc_info, &depth_image.image, &depth_image.allocation,
		nullptr);

	VkImageViewCreateInfo d_view_info = rhi_helpers::img_view_create_info(depth_image.image_format, depth_image.image,
		VK_IMAGE_ASPECT_DEPTH_BIT);

	result = vkCreateImageView(device_.value(), &d_view_info, nullptr, &depth_image.image_view);
	if (result != VK_SUCCESS) return result;
	depth_image_ = depth_image;

	return result;
}

VkResult rhi::init_descriptors()
{
	std::vector<descriptor_allocator::pool_size_ratio> sizes = {
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
	};

	const VkDevice device = device_.value();

	{
		descriptor_allocator allocator = {};
		allocator.init_pool(device, 10, sizes);
		global_descriptor_allocator_ = allocator;
		descriptor_layout_builder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		const auto [result, set] = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
		if (result != VK_SUCCESS)
		{
			return result;
		}
		draw_image_descriptor_layout_ = set;
	}
	{
		const auto [result, set] = global_descriptor_allocator_.value().allocate(
			device, draw_image_descriptor_layout_.value());
		draw_image_descriptors_ = set;
	}

	const VkDescriptorImageInfo img_info = rhi_helpers::create_img_descriptor_info(draw_image_.value().image_view);

	VkWriteDescriptorSet draw_image_write;
	draw_image_write = rhi_helpers::create_img_write_descriptor_set(draw_image_descriptors_.value(), 0, img_info);
	vkUpdateDescriptorSets(device, 1, &draw_image_write, 0, nullptr);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		std::vector<descriptor_allocator_growable::pool_size_ratio> frame_sizes = {
			{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
		};

		frame_datas_[i].frame_descriptors = descriptor_allocator_growable{};
		frame_datas_[i].frame_descriptors.value().init(device, 1000, frame_sizes);
	}
	{
		descriptor_layout_builder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		auto [result, set] = builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
		if (result != VK_SUCCESS) return result;
		gpu_scene_data_descriptor_layout_ = set;
	} {
		descriptor_layout_builder builder;
		builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		auto [result, set] = builder.build(device, VK_SHADER_STAGE_FRAGMENT_BIT);
		if (result != VK_SUCCESS) return result;
		single_image_descriptor_layout_ = set;
	}
	return VK_SUCCESS;
}

void rhi::init_allocator()
{
	VmaVulkanFunctions vulkan_functions = {};
	vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocator_create_info = {};
	allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_3;
	allocator_create_info.physicalDevice = physical_device_.value();
	allocator_create_info.device = device_.value();
	allocator_create_info.instance = instance_.value();
	allocator_create_info.pVulkanFunctions = &vulkan_functions;
	allocator_create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	VmaAllocator allocator;
	vmaCreateAllocator(&allocator_create_info, &allocator);
	allocator_ = allocator;
}

VkResult rhi::init_command_pool()
{
	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = queue_index_;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkCommandPool command_pool;
		if (const VkResult result = vkCreateCommandPool(device_.value(), &pool_info, nullptr, &command_pool); result !=
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
		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = frame_datas_[i].command_pool.value();
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VkCommandBuffer command_buffer;
		if (const VkResult result = vkAllocateCommandBuffers(device_.value(), &alloc_info, &command_buffer); result !=
			VK_SUCCESS)
			return result;
		frame_datas_[i].command_buffer = command_buffer;
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
	const VkDevice device = device_.value();
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
		VkFence fence;
		result = vkCreateFence(device, &fence_info, nullptr, &fence);
		if (result != VK_SUCCESS) return result;
		frame_datas_[i].in_flight_fence = fence;
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

	const VkDevice device = device_.value();
	VkCommandPool command_pool;
	VkResult result = vkCreateCommandPool(device, &pool_info, nullptr, &command_pool);
	if (result != VK_SUCCESS) return result;
	imm_command_pool_ = command_pool;

	// allocate the command buffer for immediate submits
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

VkResult rhi::init_graphics()
{
	std::vector<char> vert_shader_code;
	std::vector<char> frag_shader_code;
	try
	{
		vert_shader_code = read_file("out/vert.spv");
		frag_shader_code = read_file("out/tex_image.frag.spv");
	}
	catch (const std::exception& e)
	{
		rosy_utils::debug_print_a("error reading shader files! %s", e.what());
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}

	shader_pipeline sp = {};
	sp.layouts = &single_image_descriptor_layout_.value();
	sp.num_layouts = 1;
	sp.name = "test";
	sp.with_shaders(vert_shader_code, frag_shader_code);
	if (const VkResult result = sp.build(device_.value()); result != VK_SUCCESS) return result;
	test_mesh_pipeline_ = sp;
	return VK_SUCCESS;
}

VkResult rhi::init_default_data()
{
	// ReSharper disable once StringLiteralTypo
	if (auto load_result = load_gltf_meshes(this, "assets\\basicmesh.glb"); load_result.has_value())
	{
		test_meshes_ = load_result.value();
	}


	const uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	const uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	const uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	const uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));

	{
		auto [result, image] = create_image((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_SAMPLED_BIT);
		if (result != VK_SUCCESS) return result;
		white_image_ = image;
	}
	{
		auto [result, image] = create_image((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_SAMPLED_BIT);
		if (result != VK_SUCCESS) return result;
		grey_image_ = image;
	}
	{
		auto [result, image] = create_image((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_SAMPLED_BIT);
		if (result != VK_SUCCESS) return result;
		black_image_ = image;
	}
	{
		//checkerboard image
		constexpr size_t image_dimensions = static_cast<size_t>(16) * 16;
		std::array<uint32_t, image_dimensions > pixels;
		for (int x = 0; x < 16; x++) {
			for (int y = 0; y < 16; y++) {
				pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
			}
		}
		auto [result, image] = create_image(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_SAMPLED_BIT);
		if (result != VK_SUCCESS) return result;
		error_checkerboard_image_ = image;

	}

	const VkDevice device = device_.value();
	{
		VkSamplerCreateInfo sample = {};
		sample.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sample.magFilter = VK_FILTER_NEAREST;
		sample.minFilter = VK_FILTER_NEAREST;
		VkSampler sampler;
		vkCreateSampler(device, &sample, nullptr, &sampler);
		default_sampler_nearest_ = sampler;
	}
	{
		VkSamplerCreateInfo sample = {};
		sample.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sample.magFilter = VK_FILTER_LINEAR;
		sample.minFilter = VK_FILTER_LINEAR;
		VkSampler sampler;
		vkCreateSampler(device, &sample, nullptr, &sampler);
		default_sampler_linear_ = sampler;
	}

	return VK_SUCCESS;
}

VkResult rhi::init_buffer()
{
	buffer = std::unique_ptr<rhi_buffer>(new rhi_buffer{this});
	return VK_SUCCESS;
}
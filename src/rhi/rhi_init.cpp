#include "volk/volk.h"
#include "vma/vk_mem_alloc.h"
#include "RHI.h"
#include "rhi_utils.h"
#include <array>

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
		if (fd.image_available_semaphore.has_value()) vkDestroySemaphore(device, fd.image_available_semaphore.value(), nullptr);
		if (fd.render_finished_semaphore.has_value()) vkDestroySemaphore(device, fd.render_finished_semaphore.value(), nullptr);
		if (fd.command_pool.has_value()) vkDestroyCommandPool(device, fd.command_pool.value(), nullptr);
		if (fd.frame_descriptors.has_value()) fd.frame_descriptors.value().destroy_pools(device);
	}

	if (shader_pl_.has_value())
	{
		vkDestroyPipelineLayout(device_.value(), shader_pl_.value(), nullptr);
	}

	for (const VkShaderEXT shader : shaders_)
	{
		vkDestroyShaderEXT(device_.value(), shader, nullptr);
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
		if (const VkResult result = vkDeviceWaitIdle(device_.value()); result == VK_SUCCESS) vkDestroyDevice(device_.value(), nullptr);
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
			rosy_utils::debug_print_a("pushing back required SDL instance extension with name: %s\n", extensionNames[i]);
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
				requiredInstanceExtensions.erase(
					std::remove(requiredInstanceExtensions.begin(), requiredInstanceExtensions.end(), extensionName),
					requiredInstanceExtensions.end());
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
				requiredDeviceExtensions.erase(
					std::remove(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end(), extensionName),
					requiredDeviceExtensions.end());
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

	VkDebugUtilsMessengerCreateInfoEXT createInfo = createDebugCallbackInfo();
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

	VkDebugUtilsMessengerCreateInfoEXT createDebugCallackInfo = createDebugCallbackInfo();
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&createDebugCallackInfo;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledLayerCount = instance_layer_properties_.size();
	createInfo.ppEnabledLayerNames = instance_layer_properties_.data();
	createInfo.enabledExtensionCount = instance_extensions_.size();
	createInfo.ppEnabledExtensionNames = instance_extensions_.data();

	VkInstance instance;
	VkResult result = vkCreateInstance(&createInfo, NULL, &instance);
	if (result != VK_SUCCESS) return result;
	OutputDebugStringW(L"Vulkan instance created successfully!\n");
	volkLoadInstance(instance);
	instance_ = instance;
	return result;
}

VkResult rhi::init_physical_device()
{
	if (!instance_.has_value()) return VK_NOT_READY;
	std::vector<VkPhysicalDevice> physicalDevices;

	uint32_t physicalDeviceCount = 0;
	VkResult result = vkEnumeratePhysicalDevices(instance_.value(), &physicalDeviceCount, nullptr);

	physicalDevices.resize(physicalDeviceCount);
	vkEnumeratePhysicalDevices(instance_.value(), &physicalDeviceCount, &physicalDevices[0]);
	bool foundDevice = false;
	for (const VkPhysicalDevice& p_device : physicalDevices)
	{
		// get device properties
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(p_device, &deviceProperties);

		bool swapChainAdequate = false;
		swap_chain_support_details swapChainSupport = query_swap_chain_support(p_device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.present_modes.empty();
		if (!swapChainAdequate) continue;


		// shader objects required
		VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures = {};
		shaderObjectFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
		shaderObjectFeatures.pNext = nullptr;

		VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
		deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		deviceFeatures2.pNext = &shaderObjectFeatures;
		vkGetPhysicalDeviceFeatures2(p_device, &deviceFeatures2);

		if (!shaderObjectFeatures.shaderObject) continue;


		// dynamic rendering required
		VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures = {};
		bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
		bufferDeviceAddressFeatures.pNext = nullptr;

		deviceFeatures2 = {};
		deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		deviceFeatures2.pNext = &bufferDeviceAddressFeatures;
		vkGetPhysicalDeviceFeatures2(p_device, &deviceFeatures2);

		if (!bufferDeviceAddressFeatures.bufferDeviceAddress) continue;


		// buffer device address required
		VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures = {};
		dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
		dynamicRenderingFeatures.pNext = nullptr;

		deviceFeatures2 = {};
		deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		deviceFeatures2.pNext = &dynamicRenderingFeatures;
		vkGetPhysicalDeviceFeatures2(p_device, &deviceFeatures2);

		if (!dynamicRenderingFeatures.dynamicRendering) continue;


		if (deviceProperties.vendorID == cfg_.device_vendor)
		{
			{
				foundDevice = true;
				physical_device_ = p_device;
				physical_device_properties_ = deviceProperties;
			}
			{
				// features
				VkPhysicalDeviceFeatures features;
				vkGetPhysicalDeviceFeatures(p_device, &features);
				supported_features_ = features;
			}
			{
				// memory
				VkPhysicalDeviceMemoryProperties memProps;
				vkGetPhysicalDeviceMemoryProperties(p_device, &memProps);
				physical_device_memory_properties_ = memProps;
			}
			{
				// queues
				uint32_t queueCount = 0;
				vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queueCount, nullptr);
				std::vector<VkQueueFamilyProperties> queueFamilyPropertiesData;
				queueFamilyPropertiesData.resize(queueCount);
				vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queueCount, &queueFamilyPropertiesData[0]);
				queue_family_properties_ = queueFamilyPropertiesData;
			}
		}
	}
	if (!foundDevice) return VK_ERROR_FEATURE_NOT_PRESENT;
	uint32_t queueCount = 0;
	uint32_t queueIndex = 0;
	VkPhysicalDeviceFeatures supportedFeaturesData = supported_features_.value_or(requiredFeatures);
	requiredFeatures.multiDrawIndirect = VK_TRUE;
	requiredFeatures.tessellationShader = VK_TRUE;
	requiredFeatures.geometryShader = VK_TRUE;
	required_features_ = requiredFeatures;
	if (!physical_device_.has_value()) return VK_NOT_READY;

	VkPhysicalDevice p_device = physical_device_.value();
	std::vector<VkQueueFamilyProperties> queueFamilyPropertiesData = queue_family_properties_.value();
	bool foundQueue = false;
	for (std::uint32_t i = 0; i < queueFamilyPropertiesData.size(); ++i)
	{
		VkQueueFamilyProperties qfmp = queueFamilyPropertiesData[i];
		if (qfmp.timestampValidBits < 64) continue;
		if (!(qfmp.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT |
			VK_QUEUE_SPARSE_BINDING_BIT))) continue;
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(p_device, i, surface_.value(), &presentSupport);
		if (!presentSupport) continue;
		if (qfmp.queueCount > queueCount)
		{
			foundQueue = true;
			queueIndex = i;
			queueCount = qfmp.queueCount;
		}
	}
	if (!foundQueue)
	{
		rosy_utils::debug_print_a("No suitable queue found!");
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}
	queue_index_ = queueIndex;
	queue_count_ = queueCount;
	OutputDebugStringW(L"Vulkan physical device created successfully!\n");
	return result;
}

VkResult rhi::init_device()
{
	if (!physical_device_.has_value()) return VK_NOT_READY;

	VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
	deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo.pNext = nullptr;
	deviceQueueCreateInfo.flags = 0;
	deviceQueueCreateInfo.queueFamilyIndex = queue_index_;
	queue_priorities_.resize(queue_count_, 0.5f);
	deviceQueueCreateInfo.pQueuePriorities = queue_priorities_.data();
	deviceQueueCreateInfo.queueCount = queue_count_;
	VkDeviceCreateInfo deviceCreateInfo = {};

	VkPhysicalDeviceVulkan13Features vulkan13Features = {};
	vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	vulkan13Features.pNext = nullptr;
	vulkan13Features.dynamicRendering = true;
	vulkan13Features.synchronization2 = true;

	VkPhysicalDeviceVulkan12Features vulkan12Features = {};
	vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	vulkan12Features.pNext = &vulkan13Features;
	vulkan12Features.bufferDeviceAddress = true;
	vulkan12Features.descriptorIndexing = true;

	VkPhysicalDeviceShaderObjectFeaturesEXT enableShaderObject = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,
		.pNext = &vulkan12Features,
		.shaderObject = VK_TRUE
	};

	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &enableShaderObject;
	deviceCreateInfo.flags = 0;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
	deviceCreateInfo.enabledLayerCount = 0;
	deviceCreateInfo.ppEnabledLayerNames = nullptr;
	deviceCreateInfo.enabledExtensionCount = device_device_extensions_.size();
	deviceCreateInfo.ppEnabledExtensionNames = device_device_extensions_.data();
	deviceCreateInfo.pEnabledFeatures = &required_features_;
	VkDevice device;
	VkResult result = vkCreateDevice(physical_device_.value(), &deviceCreateInfo, nullptr, &device);
	if (result != VK_SUCCESS) return result;

	rosy_utils::debug_print_w(L"Vulkan device created successfully!\n");
	device_ = device;
	return result;
}

VkResult rhi::init_presentation_queue()
{
	VkQueue queue;
	VkDeviceQueueInfo2 getInfo = {};
	getInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	getInfo.flags = 0;
	getInfo.queueFamilyIndex = queue_index_;
	getInfo.queueIndex = 0;
	vkGetDeviceQueue2(device_.value(), &getInfo, &queue);
	present_queue_ = queue;
	return VK_SUCCESS;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const auto& availableFormat : availableFormats)
	{
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace ==
			VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}
	return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
	for (const auto& availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window)
{
	uint32_t max_u32 = (std::numeric_limits<uint32_t>::max)();
	if (capabilities.currentExtent.width != max_u32)
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width, height;
		SDL_GetWindowSizeInPixels(window, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
			capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
			capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

VkResult rhi::init_swap_chain(SDL_Window* window)
{
	return create_swapchain(window, VK_NULL_HANDLE);
}

VkResult rhi::create_swapchain(SDL_Window* window, const VkSwapchainKHR old_swapchain)
{
	swapchain_details_ = query_swap_chain_support(physical_device_.value());

	swapchain_image_format_ = chooseSwapSurfaceFormat(swapchain_details_.formats);
	swapchain_present_mode_ = chooseSwapPresentMode(swapchain_details_.present_modes);

	swap_chain_image_count_ = swapchain_details_.capabilities.minImageCount;
	if (swapchain_details_.capabilities.maxImageCount > 0 && swap_chain_image_count_ > swapchain_details_.capabilities.
		maxImageCount)
	{
		swap_chain_image_count_ = swapchain_details_.capabilities.maxImageCount;
	}
	frame_datas_.resize(std::min(static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT), swap_chain_image_count_));
	const VkExtent2D extent = chooseSwapExtent(swapchain_details_.capabilities, window);
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

	VkSwapchainKHR swapchain;
	VkDevice device = device_.value();
	VkResult result = vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain);
	if (result != VK_SUCCESS) return result;

	if (old_swapchain != VK_NULL_HANDLE)
	{
		destroy_swapchain();
	}

	swapchain_extent_ = extent;
	swapchain_ = swapchain;

	swap_chain_images_.clear();
	vkGetSwapchainImagesKHR(device, swapchain, &swap_chain_image_count_, nullptr);
	swap_chain_images_.resize(swap_chain_image_count_);
	vkGetSwapchainImagesKHR(device, swapchain, &swap_chain_image_count_, swap_chain_images_.data());

	for (size_t i = 0; i < swap_chain_images_.size(); i++)
	{
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swap_chain_images_[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapchain_image_format_.format;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;
		VkImageView imageView;
		VkResult result = vkCreateImageView(device, &createInfo, nullptr, &imageView);
		if (result != VK_SUCCESS) return result;
		// don't initially size these so we can clean this up nicely if any fail
		swap_chain_image_views_.push_back(imageView);
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

	VkImageCreateInfo draw_info = img_create_info(draw_image.image_format, draw_image_usages, draw_image_extent);

	VmaAllocationCreateInfo r_img_alloc_info = {};
	r_img_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	r_img_alloc_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(allocator_.value(), &draw_info, &r_img_alloc_info, &draw_image.image, &draw_image.allocation, nullptr);

	VkImageViewCreateInfo r_view_info = img_view_create_info(draw_image.image_format, draw_image.image,
		VK_IMAGE_ASPECT_COLOR_BIT);

	result = vkCreateImageView(device_.value(), &r_view_info, nullptr, &draw_image.image_view);
	if (result != VK_SUCCESS) return result;
	draw_image_ = draw_image;

	allocated_image depth_image = {};
	depth_image.image_format = VK_FORMAT_D32_SFLOAT;
	depth_image.image_extent = draw_image_extent;
	VkImageUsageFlags depth_image_usages{};
	depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo depth_info = img_create_info(depth_image.image_format, depth_image_usages, draw_image_extent);

	vmaCreateImage(allocator_.value(), &depth_info, &r_img_alloc_info, &depth_image.image, &depth_image.allocation,
		nullptr);

	VkImageViewCreateInfo d_view_info = img_view_create_info(depth_image.image_format, depth_image.image,
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
		const auto [result, set] = global_descriptor_allocator_.value().allocate(device, draw_image_descriptor_layout_.value());
		draw_image_descriptors_ = set;
	}

	VkDescriptorImageInfo img_info = {};
	img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	img_info.imageView = draw_image_.value().image_view;

	VkWriteDescriptorSet draw_image_write = {};
	draw_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	draw_image_write.pNext = nullptr;
	draw_image_write.dstBinding = 0;
	draw_image_write.dstSet = draw_image_descriptors_.value();
	draw_image_write.descriptorCount = 1;
	draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	draw_image_write.pImageInfo = &img_info;

	vkUpdateDescriptorSets(device, 1, &draw_image_write, 0, nullptr);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		std::vector<descriptor_allocator_growable::pool_size_ratio> frame_sizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		frame_datas_[i].frame_descriptors = descriptor_allocator_growable{};
		frame_datas_[i].frame_descriptors.value().init(device, 1000, frame_sizes);

	}
	return VK_SUCCESS;
}

void rhi::init_allocator()
{
	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	allocatorCreateInfo.physicalDevice = physical_device_.value();
	allocatorCreateInfo.device = device_.value();
	allocatorCreateInfo.instance = instance_.value();
	allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
	allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	VmaAllocator allocator;
	vmaCreateAllocator(&allocatorCreateInfo, &allocator);
	allocator_ = allocator;
}


VkResult rhi::init_graphics()
{
	std::vector<char> vertShaderCode;
	std::vector<char> fragShaderCode;
	try
	{
		vertShaderCode = readFile("out/vert.spv");
		fragShaderCode = readFile("out/frag.spv");
	}
	catch (const std::exception& e)
	{
		rosy_utils::debug_print_a("error reading shader files! %s", e.what());
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}

	VkResult result;
	result = create_shader_objects(vertShaderCode, fragShaderCode);
	return result;
}

VkResult rhi::create_shader_objects(const std::vector<char>& vert, const std::vector<char>& frag)
{
	VkPushConstantRange pushContantRange = {};
	pushContantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushContantRange.offset = 0;
	pushContantRange.size = sizeof(gpu_draw_push_constants);

	VkShaderCreateInfoEXT shaderCreateInfos[2] =
	{
		{
			.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
			.pNext = nullptr,
			.flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.nextStage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
			.codeSize = vert.size(),
			.pCode = vert.data(),
			.pName = "main",
			.setLayoutCount = 0,
			.pSetLayouts = nullptr,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pushContantRange,
			.pSpecializationInfo = nullptr
		},
		{
			.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
			.pNext = nullptr,
			.flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.nextStage = 0,
			.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
			.codeSize = frag.size(),
			.pCode = frag.data(),
			.pName = "main",
			.setLayoutCount = 0,
			.pSetLayouts = nullptr,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pushContantRange,
			.pSpecializationInfo = nullptr
		}
	};
	VkResult result;
	shaders_.resize(2);
	result = vkCreateShadersEXT(device_.value(), 2, shaderCreateInfos, nullptr, shaders_.data());
	VkDebugUtilsObjectNameInfoEXT vertexName = {};
	vertexName.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	vertexName.pNext = nullptr;
	vertexName.objectType = VK_OBJECT_TYPE_SHADER_EXT;
	vertexName.objectHandle = (uint64_t)shaders_[0];
	vertexName.pObjectName = "vertex";

	VkDebugUtilsObjectNameInfoEXT fragName = {};
	fragName.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	fragName.pNext = nullptr;
	fragName.objectType = VK_OBJECT_TYPE_SHADER_EXT;
	fragName.objectHandle = (uint64_t)shaders_[0];
	fragName.pObjectName = "frag";

	VkPipelineLayoutCreateInfo plInfo = {};
	plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plInfo.pNext = nullptr;
	plInfo.pushConstantRangeCount = 1;
	plInfo.pPushConstantRanges = &pushContantRange;
	VkPipelineLayout layout;
	result = vkCreatePipelineLayout(device_.value(), &plInfo, nullptr, &layout);
	shader_pl_ = layout;

	return result;
}

VkResult rhi::init_command_pool()
{

	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = queue_index_;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VkCommandPool command_pool;
		if (const VkResult result = vkCreateCommandPool(device_.value(), &pool_info, nullptr, &command_pool); result != VK_SUCCESS) return result;
		frame_datas_[i].command_pool = command_pool;
	}
	return VK_SUCCESS;
}

VkResult rhi::init_command_buffers()
{
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = frame_datas_[i].command_pool.value();
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		VkCommandBuffer command_buffer;
		if (const VkResult result = vkAllocateCommandBuffers(device_.value(), &alloc_info, &command_buffer); result != VK_SUCCESS) return result;
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

VkResult rhi::init_default_data()
{
	auto result = load_gltf_meshes(this, "assets\\basicmesh.glb");
	if (result.has_value())
	{
		test_meshes_ = result.value();
	}
	return VK_SUCCESS;
}

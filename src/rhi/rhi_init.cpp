#define VK_USE_PLATFORM_WIN32_KHR
#include "volk/volk.h"
#define VMA_IMPLEMENTATION
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

Rhi::Rhi(rosy_config::Config cfg) : m_cfg{cfg}, m_requiredFeatures{requiredFeatures}
{
	memset(&m_requiredFeatures, 0, sizeof(VkPhysicalDeviceFeatures));
}

Rhi::~Rhi()
{
	deinit();
}

VkResult Rhi::init(SDL_Window* window)
{
	VkResult result;
	result = volkInitialize();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed initialize volk! %d\n", result);
		return result;
	}
	result = this->query_instance_layers();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to query instance layers! %d\n", result);
		return result;
	}
	result = this->query_instance_extensions();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to query instance extensions! %d\n", result);
		return result;
	}
	result = this->init_instance();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to create Vulkan instance! %d\n", result);
		return result;
	}
	result = this->create_debug_callback();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to create Vulkan debug callback! %d", result);
		return result;
	}
	result = this->init_surface(window);
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to create surface! %d", result);
		return result;
	}
	result = this->init_physical_device();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to create Vulkan physical device! %d\n", result);
		return result;
	}
	result = this->query_device_layers();
	rosy_utils::DebugPrintW(L"Failed to query device layers! %d\n", result);
	if (result != VK_SUCCESS)
	{
		return result;
	}
	result = this->query_device_extensions();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to query device extensions! %d\n", result);
		return result;
	}
	result = this->init_device();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to create Vulkan device! %d\n", result);
		return result;
	}
	this->init_allocator();
	result = this->init_presentation_queue();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to get presentation queue! %d\n", result);
		return result;
	}
	result = this->init_swap_chain(window);
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to init swap chain! %d\n", result);
		return result;
	}
	result = this->init_draw_image();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to init draw image! %d\n", result);
		return result;
	}
	result = this->init_descriptors();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to init draw descriptors! %d\n", result);
		return result;
	}
	result = this->init_graphics();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to init graphics! %d\n", result);
		return result;
	}
	result = this->init_command_pool();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to init command pool! %d\n", result);
		return result;
	}
	result = this->init_command_buffers();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to init command buffers! %d\n", result);
		return result;
	}
	result = this->init_sync_objects();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to init sync objects! %d\n", result);
		return result;
	}
	result = this->initUI(window);
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to init UI! %d\n", result);
		return result;
	}
	result = this->init_commands();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to init commands! %d\n", result);
		return result;
	}
	result = this->init_default_data();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to init default data! %d\n", result);
		return result;
	}
	return VK_SUCCESS;
}

void Rhi::deinit()
{
	if (m_deinited) return;
	m_deinited = true;
	{
		// Wait for everything to be done.
		if (m_device.has_value())
		{
			vkDeviceWaitIdle(m_device.value());
		}
	}

	// Deinit begin in the reverse order from how it was created.
	deinitUI();

	for (std::shared_ptr<MeshAsset> mesh : m_testMeshes)
	{
		GPUMeshBuffers rectangle = mesh.get()->meshBuffers;
		destroyBuffer(rectangle.vertexBuffer);
		destroyBuffer(rectangle.indexBuffer);
		mesh.reset();
	}
	if (m_immFence.has_value())
	{
		vkDestroyFence(m_device.value(), m_immFence.value(), nullptr);
	}
	if (m_immCommandPool.has_value())
	{
		vkDestroyCommandPool(m_device.value(), m_immCommandPool.value(), nullptr);
	}
	for (VkFence fence : m_in_flight_fence_)
	{
		vkDestroyFence(m_device.value(), fence, nullptr);
	}
	for (VkSemaphore semaphore : m_image_available_semaphores_)
	{
		vkDestroySemaphore(m_device.value(), semaphore, nullptr);
	}
	for (VkSemaphore semaphore : m_render_finished_semaphores_)
	{
		vkDestroySemaphore(m_device.value(), semaphore, nullptr);
	}
	if (m_commandPool.has_value())
	{
		vkDestroyCommandPool(m_device.value(), m_commandPool.value(), nullptr);
	}
	if (m_shaderPL.has_value())
	{
		vkDestroyPipelineLayout(m_device.value(), m_shaderPL.value(), nullptr);
	}
	for (VkShaderEXT shader : m_shaders)
	{
		vkDestroyShaderEXT(m_device.value(), shader, nullptr);
	}
	if (m_depthImage.has_value())
	{
		AllocatedImage depthImage = m_depthImage.value();
		vkDestroyImageView(m_device.value(), depthImage.imageView, nullptr);
		vmaDestroyImage(m_allocator.value(), depthImage.image, depthImage.allocation);
	}
	if (m_drawImage.has_value())
	{
		AllocatedImage drawImage = m_drawImage.value();
		vkDestroyImageView(m_device.value(), drawImage.imageView, nullptr);
		vmaDestroyImage(m_allocator.value(), drawImage.image, drawImage.allocation);
	}
	destroySwapchain();
	if (m_debug_messenger_.has_value())
	{
		vkDestroyDebugUtilsMessengerEXT(m_instance.value(), m_debug_messenger_.value(), nullptr);
	}
	if (m_allocator.has_value())
	{
		vmaDestroyAllocator(m_allocator.value());
	}
	if (m_device.has_value())
	{
		VkResult result = vkDeviceWaitIdle(m_device.value());
		if (result == VK_SUCCESS) vkDestroyDevice(m_device.value(), NULL);
	}
	if (m_surface.has_value())
	{
		SDL_Vulkan_DestroySurface(m_instance.value(), m_surface.value(), nullptr);
	}
	if (m_instance.has_value())
	{
		vkDestroyInstance(m_instance.value(), NULL);
	}
}

void Rhi::destroySwapchain()
{
	vkDeviceWaitIdle(m_device.value());
	for (VkImageView imageView : m_swap_chain_image_views_)
	{
		vkDestroyImageView(m_device.value(), imageView, nullptr);
	}
	m_swap_chain_image_views_.clear();
	if (m_swapchain.has_value())
	{
		vkDestroySwapchainKHR(m_device.value(), m_swapchain.value(), nullptr);
		m_swapchain = std::nullopt;
	}
}

VkResult Rhi::resizeSwapchain(SDL_Window* window)
{
	vkDeviceWaitIdle(m_device.value());
	destroySwapchain();
	return create_swapchain(window, VK_NULL_HANDLE);
}

VkResult Rhi::drawFrame()
{
	VkResult result;
	result = this->render_frame();
	if (result != VK_SUCCESS)
	{
		rosy_utils::DebugPrintW(L"Failed to record command buffer! %d\n", result);
		return result;
	}
	return VK_SUCCESS;
}

VkResult Rhi::query_instance_layers()
{
	uint32_t pPropertyCount = 0;
	VkResult result = vkEnumerateInstanceLayerProperties(&pPropertyCount, nullptr);
	if (result != VK_SUCCESS) return result;
	rosy_utils::DebugPrintA("Found %d instance layers\n", pPropertyCount);
	if (pPropertyCount == 0) return result;
	std::vector<VkLayerProperties> layers;
	layers.resize(pPropertyCount);
	result = vkEnumerateInstanceLayerProperties(&pPropertyCount, layers.data());
	if (result != VK_SUCCESS) return result;
	if (!m_cfg.enable_validation_layers) return result;
	for (VkLayerProperties lp : layers)
	{
		rosy_utils::DebugPrintA("Instance layer name: %s layer description: %s\n", lp.layerName, lp.description);
		for (const char* layerName : instanceLayers)
		{
			if (strcmp(layerName, lp.layerName) == 0)
			{
				rosy_utils::DebugPrintA("\tAdding instance layer: %s\n", lp.layerName);
				m_instanceLayerProperties.push_back(layerName);
			}
		}
	}
	return result;
}

VkResult Rhi::query_device_layers()
{
	if (!m_physicalDevice.has_value()) return VK_NOT_READY;
	uint32_t pPropertyCount = 0;
	VkResult result = vkEnumerateDeviceLayerProperties(m_physicalDevice.value(), &pPropertyCount, nullptr);
	if (result != VK_SUCCESS) return result;
	rosy_utils::DebugPrintA("Found %d device layers\n", pPropertyCount);
	if (pPropertyCount == 0) return result;
	std::vector<VkLayerProperties> layers;
	layers.resize(pPropertyCount);
	result = vkEnumerateDeviceLayerProperties(m_physicalDevice.value(), &pPropertyCount, layers.data());
	if (result != VK_SUCCESS) return result;
	for (VkLayerProperties lp : layers)
	{
		rosy_utils::DebugPrintA("Device layer name: %s layer description: %s\n", lp.layerName, lp.description);
	}
	return result;
}

VkResult Rhi::query_instance_extensions()
{
	uint32_t pPropertyCount = 0;
	VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &pPropertyCount, nullptr);
	if (result != VK_SUCCESS) return result;

	rosy_utils::DebugPrintA("Found %d instance extensions\n", pPropertyCount);
	if (pPropertyCount == 0) return result;

	std::vector<VkExtensionProperties> extensions;
	extensions.resize(pPropertyCount);
	result = vkEnumerateInstanceExtensionProperties(nullptr, &pPropertyCount, extensions.data());
	if (result != VK_SUCCESS) return result;
	rosy_utils::DebugPrintA("num required instance extensions: %d\n", std::size(instanceExtensions));
	{
		// Setup required instance extensions
		size_t found_extensions = 0;
		uint32_t extensionCount;
		auto extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
		for (uint32_t i = 0; i < extensionCount; i++)
		{
			rosy_utils::DebugPrintA("pushing back required SDL instance extension with name: %s\n", extensionNames[i]);
			m_instanceExtensions.push_back(extensionNames[i]);
		}
		for (uint32_t i = 0; i < std::size(instanceExtensions); i++)
		{
			rosy_utils::DebugPrintA("pushing back required rosy instance extension with name: %s\n",
			                        instanceExtensions[i]);
			m_instanceExtensions.push_back(instanceExtensions[i]);
		}
	}
	rosy_utils::DebugPrintA("num m_instanceExtensions: %d\n", m_instanceExtensions.size());

	std::vector<const char*> requiredInstanceExtensions(std::begin(m_instanceExtensions),
	                                                    std::end(m_instanceExtensions));
	for (VkExtensionProperties ep : extensions)
	{
		rosy_utils::DebugPrintA("Instance extension name: %s\n", ep.extensionName);
		for (const char* extensionName : m_instanceExtensions)
		{
			if (strcmp(extensionName, ep.extensionName) == 0)
			{
				rosy_utils::DebugPrintA("\tRequiring instance extension: %s\n", extensionName);
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

VkResult Rhi::query_device_extensions()
{
	uint32_t pPropertyCount = 0;
	if (!m_physicalDevice.has_value()) return VK_NOT_READY;

	VkResult result = vkEnumerateDeviceExtensionProperties(m_physicalDevice.value(), nullptr, &pPropertyCount, nullptr);
	if (result != VK_SUCCESS) return result;

	rosy_utils::DebugPrintA("Found %d device extensions\n", pPropertyCount);
	if (pPropertyCount == 0) return result;

	std::vector<VkExtensionProperties> extensions;
	extensions.resize(pPropertyCount);

	result = vkEnumerateDeviceExtensionProperties(m_physicalDevice.value(), nullptr, &pPropertyCount,
	                                              extensions.data());
	if (result != VK_SUCCESS) return result;

	// validate required device extensions
	std::vector<const char*> requiredDeviceExtensions(std::begin(deviceExtensions), std::end(deviceExtensions));

	for (VkExtensionProperties ep : extensions)
	{
		rosy_utils::DebugPrintA("Device extension name: %s\n", ep.extensionName);
		for (const char* extensionName : deviceExtensions)
		{
			if (strcmp(extensionName, ep.extensionName) == 0)
			{
				rosy_utils::DebugPrintA("\tRequiring device extension: %s\n", extensionName);
				m_deviceDeviceExtensions.push_back(extensionName);
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


VkResult Rhi::create_debug_callback()
{
	if (!m_cfg.enable_validation_layers) return VK_SUCCESS;

	VkDebugUtilsMessengerCreateInfoEXT createInfo = createDebugCallbackInfo();
	VkDebugUtilsMessengerEXT debugMessenger;
	VkResult result = vkCreateDebugUtilsMessengerEXT(m_instance.value(), &createInfo, nullptr, &debugMessenger);
	if (result != VK_SUCCESS) return result;
	m_debug_messenger_ = debugMessenger;
	return result;
}

VkResult Rhi::init_surface(SDL_Window* window)
{
	VkSurfaceKHR surface;
	SDL_Vulkan_CreateSurface(window, m_instance.value(), nullptr, &surface);
	m_surface = surface;
	return VK_SUCCESS;
}

VkResult Rhi::init_instance()
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
	createInfo.enabledLayerCount = m_instanceLayerProperties.size();
	createInfo.ppEnabledLayerNames = m_instanceLayerProperties.data();
	createInfo.enabledExtensionCount = m_instanceExtensions.size();
	createInfo.ppEnabledExtensionNames = m_instanceExtensions.data();

	VkInstance instance;
	VkResult result = vkCreateInstance(&createInfo, NULL, &instance);
	if (result != VK_SUCCESS) return result;
	OutputDebugStringW(L"Vulkan instance created successfully!\n");
	volkLoadInstance(instance);
	m_instance = instance;
	return result;
}

VkResult Rhi::init_physical_device()
{
	if (!m_instance.has_value()) return VK_NOT_READY;
	std::vector<VkPhysicalDevice> physicalDevices;

	uint32_t physicalDeviceCount = 0;
	VkResult result = vkEnumeratePhysicalDevices(m_instance.value(), &physicalDeviceCount, nullptr);

	physicalDevices.resize(physicalDeviceCount);
	vkEnumeratePhysicalDevices(m_instance.value(), &physicalDeviceCount, &physicalDevices[0]);
	bool foundDevice = false;
	for (const VkPhysicalDevice& p_device : physicalDevices)
	{
		// get device properties
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(p_device, &deviceProperties);

		bool swapChainAdequate = false;
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(p_device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
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


		if (deviceProperties.vendorID == m_cfg.device_vendor)
		{
			{
				foundDevice = true;
				m_physicalDevice = p_device;
				m_physicalDeviceProperties = deviceProperties;
			}
			{
				// features
				VkPhysicalDeviceFeatures features;
				vkGetPhysicalDeviceFeatures(p_device, &features);
				m_supportedFeatures = features;
			}
			{
				// memory
				VkPhysicalDeviceMemoryProperties memProps;
				vkGetPhysicalDeviceMemoryProperties(p_device, &memProps);
				m_physicalDeviceMemoryProperties = memProps;
			}
			{
				// queues
				uint32_t queueCount = 0;
				vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queueCount, nullptr);
				std::vector<VkQueueFamilyProperties> queueFamilyPropertiesData;
				queueFamilyPropertiesData.resize(queueCount);
				vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queueCount, &queueFamilyPropertiesData[0]);
				m_queueFamilyProperties = queueFamilyPropertiesData;
			}
		}
	}
	if (!foundDevice) return VK_ERROR_FEATURE_NOT_PRESENT;
	uint32_t queueCount = 0;
	uint32_t queueIndex = 0;
	VkPhysicalDeviceFeatures supportedFeaturesData = m_supportedFeatures.value_or(requiredFeatures);
	requiredFeatures.multiDrawIndirect = VK_TRUE;
	requiredFeatures.tessellationShader = VK_TRUE;
	requiredFeatures.geometryShader = VK_TRUE;
	m_requiredFeatures = requiredFeatures;
	if (!m_physicalDevice.has_value()) return VK_NOT_READY;

	VkPhysicalDevice p_device = m_physicalDevice.value();
	std::vector<VkQueueFamilyProperties> queueFamilyPropertiesData = m_queueFamilyProperties.value();
	bool foundQueue = false;
	for (std::uint32_t i = 0; i < queueFamilyPropertiesData.size(); ++i)
	{
		VkQueueFamilyProperties qfmp = queueFamilyPropertiesData[i];
		if (qfmp.timestampValidBits < 64) continue;
		if (!(qfmp.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT |
			VK_QUEUE_SPARSE_BINDING_BIT))) continue;
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(p_device, i, m_surface.value(), &presentSupport);
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
		rosy_utils::DebugPrintA("No suitable queue found!");
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}
	m_queueIndex = queueIndex;
	m_queueCount = queueCount;
	OutputDebugStringW(L"Vulkan physical device created successfully!\n");
	return result;
}

VkResult Rhi::init_device()
{
	if (!m_physicalDevice.has_value()) return VK_NOT_READY;

	VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
	deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo.pNext = nullptr;
	deviceQueueCreateInfo.flags = 0;
	deviceQueueCreateInfo.queueFamilyIndex = m_queueIndex;
	m_queuePriorities.resize(m_queueCount, 0.5f);
	deviceQueueCreateInfo.pQueuePriorities = m_queuePriorities.data();
	deviceQueueCreateInfo.queueCount = m_queueCount;
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
	deviceCreateInfo.enabledExtensionCount = m_deviceDeviceExtensions.size();
	deviceCreateInfo.ppEnabledExtensionNames = m_deviceDeviceExtensions.data();
	deviceCreateInfo.pEnabledFeatures = &m_requiredFeatures;
	VkDevice device;
	VkResult result = vkCreateDevice(m_physicalDevice.value(), &deviceCreateInfo, nullptr, &device);
	if (result != VK_SUCCESS) return result;

	rosy_utils::DebugPrintW(L"Vulkan device created successfully!\n");
	m_device = device;
	return result;
}

VkResult Rhi::init_presentation_queue()
{
	VkQueue queue;
	VkDeviceQueueInfo2 getInfo = {};
	getInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	getInfo.flags = 0;
	getInfo.queueFamilyIndex = m_queueIndex;
	getInfo.queueIndex = 0;
	vkGetDeviceQueue2(m_device.value(), &getInfo, &queue);
	m_presentQueue = queue;
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

VkResult Rhi::init_swap_chain(SDL_Window* window)
{
	return create_swapchain(window, VK_NULL_HANDLE);
}

VkResult Rhi::create_swapchain(SDL_Window* window, VkSwapchainKHR old_swapchain)
{
	m_swapchainDetails = querySwapChainSupport(m_physicalDevice.value());

	m_swapchainImageFormat = chooseSwapSurfaceFormat(m_swapchainDetails.formats);
	m_swapchainPresentMode = chooseSwapPresentMode(m_swapchainDetails.presentModes);

	m_swapChainImageCount = m_swapchainDetails.capabilities.minImageCount;
	if (m_swapchainDetails.capabilities.maxImageCount > 0 && m_swapChainImageCount > m_swapchainDetails.capabilities.
		maxImageCount)
	{
		m_swapChainImageCount = m_swapchainDetails.capabilities.maxImageCount;
	}
	VkExtent2D extent = chooseSwapExtent(m_swapchainDetails.capabilities, window);
	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_surface.value();
	createInfo.minImageCount = m_swapChainImageCount;
	createInfo.imageFormat = m_swapchainImageFormat.format;
	createInfo.imageColorSpace = m_swapchainImageFormat.colorSpace;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	// Just one queue family right now.
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;

	createInfo.preTransform = m_swapchainDetails.capabilities.currentTransform;

	createInfo.presentMode = m_swapchainPresentMode;
	createInfo.clipped = VK_TRUE;

	createInfo.oldSwapchain = old_swapchain;

	VkSwapchainKHR swapchain;
	VkDevice device = m_device.value();
	VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain);
	if (result != VK_SUCCESS) return result;

	if (old_swapchain != VK_NULL_HANDLE)
	{
		destroySwapchain();
	}

	m_swapchainExtent = extent;
	m_swapchain = swapchain;

	m_swap_chain_images_.clear();
	vkGetSwapchainImagesKHR(device, swapchain, &m_swapChainImageCount, nullptr);
	m_swap_chain_images_.resize(m_swapChainImageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &m_swapChainImageCount, m_swap_chain_images_.data());

	for (size_t i = 0; i < m_swap_chain_images_.size(); i++)
	{
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = m_swap_chain_images_[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = m_swapchainImageFormat.format;
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
		m_swap_chain_image_views_.push_back(imageView);
	}
	return VK_SUCCESS;
}

VkResult Rhi::init_draw_image()
{
	VkResult result;

	VkExtent3D draw_image_extent = {
		.width = static_cast<uint32_t>(m_cfg.maxWindowWidth),
		.height = static_cast<uint32_t>(m_cfg.maxWindowHeight),
		.depth = 1
	};
	AllocatedImage draw_image = {};
	draw_image.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	draw_image.imageExtent = draw_image_extent;

	VkImageUsageFlags draw_image_usages{};
	draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
	draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo draw_info = imgCreateInfo(draw_image.imageFormat, draw_image_usages, draw_image_extent);

	VmaAllocationCreateInfo r_img_alloc_info = {};
	r_img_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	r_img_alloc_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(m_allocator.value(), &draw_info, &r_img_alloc_info, &draw_image.image, &draw_image.allocation, nullptr);

	VkImageViewCreateInfo r_view_info = imgViewCreateInfo(draw_image.imageFormat, draw_image.image,
	                                                     VK_IMAGE_ASPECT_COLOR_BIT);

	result = vkCreateImageView(m_device.value(), &r_view_info, nullptr, &draw_image.imageView);
	if (result != VK_SUCCESS) return result;
	m_drawImage = draw_image;

	AllocatedImage depth_image = {};
	depth_image.imageFormat = VK_FORMAT_D32_SFLOAT;
	depth_image.imageExtent = draw_image_extent;
	VkImageUsageFlags depth_image_usages{};
	depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo depth_info = imgCreateInfo(depth_image.imageFormat, depth_image_usages, draw_image_extent);

	vmaCreateImage(m_allocator.value(), &depth_info, &r_img_alloc_info, &depth_image.image, &depth_image.allocation,
	               nullptr);

	VkImageViewCreateInfo d_view_info = imgViewCreateInfo(depth_image.imageFormat, depth_image.image,
	                                                     VK_IMAGE_ASPECT_DEPTH_BIT);

	result = vkCreateImageView(m_device.value(), &d_view_info, nullptr, &depth_image.imageView);
	if (result != VK_SUCCESS) return result;
	m_depthImage = depth_image;

	return result;
}

VkResult Rhi::init_descriptors()
{
	std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
	};

	const VkDevice device = m_device.value();

	{
		DescriptorAllocator allocator = {};
		allocator.initPool(device, 10, sizes);
		m_globalDescriptorAllocator = allocator;
		DescriptorLayoutBuilder builder;
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		const auto [result, set] = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
		if (result != VK_SUCCESS)
		{
			return result;
		}
		m_drawImageDescriptorLayout = set;
	}
	{
		const auto [result, set] = m_globalDescriptorAllocator.value().allocate(device, m_drawImageDescriptorLayout.value());
		m_drawImageDescriptors = set;
	}

	VkDescriptorImageInfo img_info{};
	img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	img_info.imageView = m_drawImage.value().imageView;

	VkWriteDescriptorSet draw_image_write = {};
	draw_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	draw_image_write.pNext = nullptr;
	draw_image_write.dstBinding = 0;
	draw_image_write.dstSet = m_drawImageDescriptors.value();
	draw_image_write.descriptorCount = 1;
	draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	draw_image_write.pImageInfo = &img_info;

	vkUpdateDescriptorSets(device, 1, &draw_image_write, 0, nullptr);

	return VK_SUCCESS;
}

void Rhi::init_allocator()
{
	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	allocatorCreateInfo.physicalDevice = m_physicalDevice.value();
	allocatorCreateInfo.device = m_device.value();
	allocatorCreateInfo.instance = m_instance.value();
	allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
	allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	VmaAllocator allocator;
	vmaCreateAllocator(&allocatorCreateInfo, &allocator);
	m_allocator = allocator;
}


VkResult Rhi::init_graphics()
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
		rosy_utils::DebugPrintA("error reading shader files! %s", e.what());
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}

	VkResult result;
	result = create_shader_objects(vertShaderCode, fragShaderCode);
	return result;
}

VkResult Rhi::create_shader_objects(const std::vector<char>& vert, const std::vector<char>& frag)
{
	VkPushConstantRange pushContantRange = {};
	pushContantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushContantRange.offset = 0;
	pushContantRange.size = sizeof(GPUDrawPushConstants);

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
	m_shaders.resize(2);
	result = vkCreateShadersEXT(m_device.value(), 2, shaderCreateInfos, nullptr, m_shaders.data());
	VkDebugUtilsObjectNameInfoEXT vertexName = {};
	vertexName.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	vertexName.pNext = nullptr;
	vertexName.objectType = VK_OBJECT_TYPE_SHADER_EXT;
	vertexName.objectHandle = (uint64_t)m_shaders[0];
	vertexName.pObjectName = "vertex";

	VkDebugUtilsObjectNameInfoEXT fragName = {};
	fragName.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	fragName.pNext = nullptr;
	fragName.objectType = VK_OBJECT_TYPE_SHADER_EXT;
	fragName.objectHandle = (uint64_t)m_shaders[0];
	fragName.pObjectName = "frag";

	VkPipelineLayoutCreateInfo plInfo = {};
	plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plInfo.pNext = nullptr;
	plInfo.pushConstantRangeCount = 1;
	plInfo.pPushConstantRanges = &pushContantRange;
	VkPipelineLayout layout;
	result = vkCreatePipelineLayout(m_device.value(), &plInfo, nullptr, &layout);
	m_shaderPL = layout;

	return result;
}

VkResult Rhi::init_command_pool()
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = m_queueIndex;

	VkCommandPool commandPool;
	VkResult result = vkCreateCommandPool(m_device.value(), &poolInfo, nullptr, &commandPool);
	if (result != VK_SUCCESS) return result;
	m_commandPool = commandPool;
	return result;
}

VkResult Rhi::init_command_buffers()
{
	m_command_buffers_.resize(MAX_FRAMES_IN_FLIGHT);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool.value();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)m_command_buffers_.size();

	VkResult result = vkAllocateCommandBuffers(m_device.value(), &allocInfo, m_command_buffers_.data());
	return result;
}

VkResult Rhi::init_sync_objects()
{
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkResult result;
	VkDevice device = m_device.value();
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkSemaphore semaphore;
		result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore);
		if (result != VK_SUCCESS) return result;
		m_image_available_semaphores_.push_back(semaphore);
		result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore);
		if (result != VK_SUCCESS) return result;
		m_render_finished_semaphores_.push_back(semaphore);
		VkFence fence;
		result = vkCreateFence(device, &fenceInfo, nullptr, &fence);
		if (result != VK_SUCCESS) return result;
		m_in_flight_fence_.push_back(fence);
	}
	{
		VkFence fence;
		result = vkCreateFence(device, &fenceInfo, nullptr, &fence);
		m_immFence = fence;
	}
	return VK_SUCCESS;
}


VkResult Rhi::init_commands()
{
	VkResult result;

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = m_queueIndex;

	VkDevice device = m_device.value();
	VkCommandPool commandPool;
	result = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
	if (result != VK_SUCCESS) return result;
	m_immCommandPool = commandPool;

	// allocate the command buffer for immediate submits
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_immCommandPool.value();
	allocInfo.commandBufferCount = 1;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VkCommandBuffer buffer;
	result = vkAllocateCommandBuffers(device, &allocInfo, &buffer);
	if (result != VK_SUCCESS) return result;
	m_immCommandBuffer = buffer;

	return VK_SUCCESS;
}

VkResult Rhi::init_default_data()
{
	auto result = load_gltf_meshes(this, "assets\\basicmesh.glb");
	if (result.has_value())
	{
		m_testMeshes = result.value();
	}
	return VK_SUCCESS;
}

#define VK_USE_PLATFORM_WIN32_KHR
#define VOLK_IMPLEMENTATION
#include "volk/volk.h"
#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"
#include "RHI.h"

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

Rhi::Rhi(rosy_config::Config cfg) :m_cfg{ cfg }, m_requiredFeatures{ requiredFeatures } {
	memset(&m_requiredFeatures, 0, sizeof(VkPhysicalDeviceFeatures));
}

VkResult Rhi::init(SDL_Window* window) {

	VkResult result;
	result = volkInitialize();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed initialize volk! %d\n", result);
		return result;
	}
	result = this->queryInstanceLayers();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to query instance layers! %d\n", result);
		return result;
	}
	result = this->queryInstanceExtensions();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to query instance extensions! %d\n", result);
		return result;
	}
	result = this->initInstance();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to create Vulkan instance! %d\n", result);
		return result;
	}
	result = this->createDebugCallback();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to create Vulkan debug callback! %d", result);
		return result;
	}
	result = this->initSurface(window);
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to create surface! %d", result);
		return result;
	}
	result = this->initPhysicalDevice();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to create Vulkan physical device! %d\n", result);
		return result;
	}
	result = this->queryDeviceLayers();
	rosy_utils::DebugPrintW(L"Failed to query device layers! %d\n", result);
	if (result != VK_SUCCESS) {
		return result;
	}
	result = this->queryDeviceExtensions();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to query device extensions! %d\n", result);
		return result;
	}
	result = this->initDevice();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to create Vulkan device! %d\n", result);
		return result;
	}
	this->initAllocator();
	result = this->initPresentationQueue();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to get presentation queue! %d\n", result);
		return result;
	}
	result = this->initSwapChain(window);
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to init swap chain! %d\n", result);
		return result;
	}
	result = this->initImageViews();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to init image views! %d\n", result);
		return result;
	}
	result = this->initGraphics();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to init graphics! %d\n", result);
		return result;
	}
	result = this->initCommandPool();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to init command pool! %d\n", result);
		return result;
	}
	result = this->initCommandBuffers();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to init command buffers! %d\n", result);
		return result;
	}
	result = this->initSyncObjects();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to init sync objects! %d\n", result);
		return result;
	}
	return VK_SUCCESS;
}

VkResult Rhi::drawFrame() {
	VkResult result;
	result = this->renderFrame();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to record command buffer! %d\n", result);
		return result;
	}
	return VK_SUCCESS;
}

VkResult Rhi::queryInstanceLayers() {
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
	for (VkLayerProperties lp : layers) {
		rosy_utils::DebugPrintA("Instance layer name: %s layer description: %s\n", lp.layerName, lp.description);
		for (const char* layerName : instanceLayers) {
			if (strcmp(layerName, lp.layerName) == 0) {
				rosy_utils::DebugPrintA("\tAdding instance layer: %s\n", lp.layerName);
				m_instanceLayerProperties.push_back(layerName);
			}
		}
	}
	return result;
}

VkResult Rhi::queryDeviceLayers() {
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
	for (VkLayerProperties lp : layers) {
		rosy_utils::DebugPrintA("Device layer name: %s layer description: %s\n", lp.layerName, lp.description);
	}
	return result;
}

VkResult Rhi::queryInstanceExtensions() {
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
		for (uint32_t i = 0; i < extensionCount; i++) {
			rosy_utils::DebugPrintA("pushing back required SDL instance extension with name: %s\n", extensionNames[i]);
			m_instanceExtensions.push_back(extensionNames[i]);
		}
		for (uint32_t i = 0; i < std::size(instanceExtensions); i++) {
			rosy_utils::DebugPrintA("pushing back required rosy instance extension with name: %s\n", instanceExtensions[i]);
			m_instanceExtensions.push_back(instanceExtensions[i]);
		}
	}
	rosy_utils::DebugPrintA("num m_instanceExtensions: %d\n", m_instanceExtensions.size());

	std::vector<const char*> requiredInstanceExtensions(std::begin(m_instanceExtensions), std::end(m_instanceExtensions));
	for (VkExtensionProperties ep : extensions) {
		rosy_utils::DebugPrintA("Instance extension name: %s\n", ep.extensionName);
		for (const char* extensionName : m_instanceExtensions) {
			if (strcmp(extensionName, ep.extensionName) == 0) {
				rosy_utils::DebugPrintA("\tRequiring instance extension: %s\n", extensionName);
				requiredInstanceExtensions.erase(std::remove(requiredInstanceExtensions.begin(), requiredInstanceExtensions.end(), extensionName), requiredInstanceExtensions.end());
			}
		}
	}
	if (requiredInstanceExtensions.size() != 0) {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
	return result;
}

VkResult Rhi::queryDeviceExtensions() {
	uint32_t pPropertyCount = 0;
	if (!m_physicalDevice.has_value()) return VK_NOT_READY;

	VkResult result = vkEnumerateDeviceExtensionProperties(m_physicalDevice.value(), nullptr, &pPropertyCount, nullptr);
	if (result != VK_SUCCESS) return result;

	rosy_utils::DebugPrintA("Found %d device extensions\n", pPropertyCount);
	if (pPropertyCount == 0) return result;

	std::vector<VkExtensionProperties> extensions;
	extensions.resize(pPropertyCount);

	result = vkEnumerateDeviceExtensionProperties(m_physicalDevice.value(), nullptr, &pPropertyCount, extensions.data());
	if (result != VK_SUCCESS) return result;

	// validate required device extensions
	std::vector<const char*> requiredDeviceExtensions(std::begin(deviceExtensions), std::end(deviceExtensions));

	for (VkExtensionProperties ep : extensions) {
		rosy_utils::DebugPrintA("Device extension name: %s\n", ep.extensionName);
		for (const char* extensionName : deviceExtensions) {
			if (strcmp(extensionName, ep.extensionName) == 0) {
				rosy_utils::DebugPrintA("\tRequiring device extension: %s\n", extensionName);
				m_deviceDeviceExtensions.push_back(extensionName);
				requiredDeviceExtensions.erase(std::remove(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end(), extensionName), requiredDeviceExtensions.end());
			}
		}
	}

	if (requiredDeviceExtensions.size() != 0) {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
	return result;
}


VkResult Rhi::createDebugCallback() {
	if (!m_cfg.enable_validation_layers) return VK_SUCCESS;

	VkDebugUtilsMessengerCreateInfoEXT createInfo = createDebugCallbackInfo();
	VkDebugUtilsMessengerEXT debugMessenger;
	VkResult result = vkCreateDebugUtilsMessengerEXT(m_instance.value(), &createInfo, nullptr, &debugMessenger);
	if (result != VK_SUCCESS) return result;
	m_debugMessenger = debugMessenger;
	return result;
}

VkResult Rhi::initSurface(SDL_Window* window) {
	VkSurfaceKHR surface;
	SDL_Vulkan_CreateSurface(window, m_instance.value(), nullptr, &surface);
	m_surface = surface;
	return VK_SUCCESS;
}

VkResult Rhi::initInstance() {
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

SwapChainSupportDetails Rhi::querySwapChainSupport(VkPhysicalDevice device) {
	SwapChainSupportDetails details = {};
	VkSurfaceKHR surface = m_surface.value();
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}
	return details;
}

VkResult Rhi::initPhysicalDevice() {
	if (!m_instance.has_value()) return VK_NOT_READY;
	std::vector<VkPhysicalDevice> physicalDevices;

	uint32_t physicalDeviceCount = 0;
	VkResult result = vkEnumeratePhysicalDevices(m_instance.value(), &physicalDeviceCount, nullptr);

	physicalDevices.resize(physicalDeviceCount);
	vkEnumeratePhysicalDevices(m_instance.value(), &physicalDeviceCount, &physicalDevices[0]);
	bool foundDevice = false;
	for (const VkPhysicalDevice& p_device : physicalDevices) {
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

		VkPhysicalDeviceFeatures2  deviceFeatures2 = {};
		deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
		deviceFeatures2.pNext = &shaderObjectFeatures;
		vkGetPhysicalDeviceFeatures2(p_device, &deviceFeatures2);

		if (!shaderObjectFeatures.shaderObject) continue;


		// dynamic rendering required
		VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures = {};
		dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
		dynamicRenderingFeatures.pNext = nullptr;

		deviceFeatures2 = {};
		deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
		deviceFeatures2.pNext = &dynamicRenderingFeatures;
		vkGetPhysicalDeviceFeatures2(p_device, &deviceFeatures2);

		if (!dynamicRenderingFeatures.dynamicRendering) continue;


		if (deviceProperties.vendorID == m_cfg.device_vendor) {
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
	for (std::uint32_t i = 0; i < queueFamilyPropertiesData.size(); ++i) {
		VkQueueFamilyProperties qfmp = queueFamilyPropertiesData[i];
		if (qfmp.timestampValidBits < 64) continue;
		if (!(qfmp.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT))) continue;
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(p_device, i, m_surface.value(), &presentSupport);
		if (!presentSupport) continue;
		if (qfmp.queueCount > queueCount) {
			foundQueue = true;
			queueIndex = i;
			queueCount = qfmp.queueCount;
		}
	}
	if (!foundQueue) {
		rosy_utils::DebugPrintA("No suitable queue found!");
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}
	m_queueIndex = queueIndex;
	m_queueCount = queueCount;
	OutputDebugStringW(L"Vulkan physical device created successfully!\n");
	return result;
}

VkResult Rhi::initDevice() {
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

VkResult Rhi::initPresentationQueue() {
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

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormat;
		}
	}
	return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window) {
	uint32_t max_u32 = (std::numeric_limits<uint32_t>::max)();
	if (capabilities.currentExtent.width != max_u32) {
		return capabilities.currentExtent;
	}
	else {
		int width, height;
		SDL_GetWindowSizeInPixels(window, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

VkResult Rhi::initSwapChain(SDL_Window* window) {
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice.value());

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, window);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_surface.value();
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	// Just one queue family right now.
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	// TODO: support window resize
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	VkSwapchainKHR swapchain;
	VkDevice device = m_device.value();
	VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain);
	if (result != VK_SUCCESS) return result;

	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
	m_swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, m_swapChainImages.data());

	m_swapChainImageFormat = surfaceFormat.format;
	m_swapChainExtent = extent;
	m_swapchain = swapchain;
	return result;
}

VkResult Rhi::initImageViews() {
	VkDevice device = m_device.value();
	for (size_t i = 0; i < m_swapChainImages.size(); i++) {
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = m_swapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = m_swapChainImageFormat;
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
		m_swapChainImageViews.push_back(imageView);
	}
	return VK_SUCCESS;
}

void Rhi::debug() {
	rosy_utils::DebugPrintA("RHI Debug Data::");
	if (!m_instance.has_value()) {
		rosy_utils::DebugPrintA("No instance!");
		return;
	}

	if (!m_physicalDeviceProperties.has_value()) {
		rosy_utils::DebugPrintA("No physical device!");
		return;
	}
	VkPhysicalDeviceProperties deviceProperties = m_physicalDeviceProperties.value();
	VkPhysicalDeviceFeatures deviceFeatures = m_supportedFeatures.value();
	VkPhysicalDeviceMemoryProperties deviceMemProps = m_physicalDeviceMemoryProperties.value();
	std::vector<VkQueueFamilyProperties> queueFamilyPropertiesData = m_queueFamilyProperties.value();
	rosy_utils::DebugPrintA("result device property vendor %s \n", deviceProperties.deviceName);
	rosy_utils::DebugPrintA("result: vendor: %u \n", deviceProperties.vendorID);

	rosy_utils::DebugPrintA("has multiDrawIndirect? %d \n", deviceFeatures.multiDrawIndirect);
	for (uint32_t i = 0; i < deviceMemProps.memoryHeapCount; i++) {
		rosy_utils::DebugPrintA("memory size: %d\n", deviceMemProps.memoryHeaps[i].size);
		rosy_utils::DebugPrintA("memory flags: %d\n", deviceMemProps.memoryHeaps[i].flags);
	}
	for (const VkQueueFamilyProperties& qfmp : queueFamilyPropertiesData) {
		rosy_utils::DebugPrintA("queue count: %d and time bits: %d\n", qfmp.queueCount, qfmp.timestampValidBits);
		if (qfmp.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT)) {
			rosy_utils::DebugPrintA("VkQueueFamilyProperties got all the things\n");
		}
		else {
			rosy_utils::DebugPrintA("VkQueueFamilyProperties missing stuff\n");
		}
	}
	rosy_utils::DebugPrintA("Selected queue index %d with count: %d\n", m_queueIndex, m_queueCount);

}

void Rhi::initAllocator() {
	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	allocatorCreateInfo.physicalDevice = m_physicalDevice.value();
	allocatorCreateInfo.device = m_device.value();
	allocatorCreateInfo.instance = m_instance.value();
	allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

	VmaAllocator allocator;
	vmaCreateAllocator(&allocatorCreateInfo, &allocator);
	m_allocator = allocator;
}


VkResult Rhi::initGraphics() {
	std::vector<char> vertShaderCode;
	std::vector<char> fragShaderCode;
	try {
		vertShaderCode = readFile("out/vert.spv");
		fragShaderCode = readFile("out/frag.spv");
	}
	catch (const std::exception& e) {
		rosy_utils::DebugPrintA("error reading shader files! %s", e.what());
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}

	VkResult result;
	result = createShaderObjects(vertShaderCode, fragShaderCode);
	return result;
}

VkResult Rhi::createShaderObjects(const std::vector<char>& vert, const std::vector<char>& frag) {
	VkDescriptorSetLayout descriptorSetLayout;
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
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr,
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
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr,
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
	return result;
}


VkResult Rhi::initCommandPool() {
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

VkResult Rhi::initCommandBuffers() {
	m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool.value();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

	VkResult result = vkAllocateCommandBuffers(m_device.value(), &allocInfo, m_commandBuffers.data());
	return result;
}

VkResult Rhi::initSyncObjects() {
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkResult result;
	VkDevice device = m_device.value();
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VkSemaphore semaphore;
		result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore);
		if (result != VK_SUCCESS) return result;
		m_imageAvailableSemaphores.push_back(semaphore);
		result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore);
		if (result != VK_SUCCESS) return result;
		m_renderFinishedSemaphores.push_back(semaphore);
		VkFence fence;
		result = vkCreateFence(device, &fenceInfo, nullptr, &fence);
		if (result != VK_SUCCESS) return result;
		m_inFlightFence.push_back(fence);
	}
	return VK_SUCCESS;
}


void Rhi::transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout) {
	VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = aspectMask;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;


	VkImageMemoryBarrier2 imageBarrier = {};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.pNext = nullptr;
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
	imageBarrier.oldLayout = currentLayout;
	imageBarrier.newLayout = newLayout;
	imageBarrier.subresourceRange = subresourceRange;
	imageBarrier.image = image;

	VkDependencyInfo dependencyInfo{};
	dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependencyInfo.pNext = nullptr;

	dependencyInfo.imageMemoryBarrierCount = 1;
	dependencyInfo.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

VkResult Rhi::renderFrame() {
	VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
	VkSemaphore imageAvailableSignal = m_imageAvailableSemaphores[m_currentFrame];
	VkSemaphore renderedFinisishedSignal = m_renderFinishedSemaphores[m_currentFrame];
	VkFence fence = m_inFlightFence[m_currentFrame];
	VkResult result;
	VkDevice device = m_device.value();

	result = vkWaitForFences(device, 1, &fence, true, 1000000000);
	if (result != VK_SUCCESS) return result;

	uint32_t imageIndex;
	vkAcquireNextImageKHR(m_device.value(), m_swapchain.value(), UINT64_MAX, imageAvailableSignal, VK_NULL_HANDLE, &imageIndex);
	VkImage image = m_swapChainImages[imageIndex];
	VkImageView imageView = m_swapChainImageViews[imageIndex];

	vkResetFences(device, 1, &fence);
	{
		// Start recording commands
		result = vkResetCommandBuffer(cmd, 0);
		if (result != VK_SUCCESS) return result;

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		result = vkBeginCommandBuffer(cmd, &beginInfo);
		if (result != VK_SUCCESS) return result;
	}


	{
		// Configure the dynamic shader pipeline
		VkExtent2D swapChainExtent = m_swapChainExtent;
		{
			vkCmdSetRasterizerDiscardEnableEXT(cmd, VK_FALSE);
			VkColorBlendEquationEXT colorBlendEquationEXT{};
			vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &colorBlendEquationEXT);
		}
		{
			vkCmdSetPrimitiveTopologyEXT(cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
			vkCmdSetPrimitiveRestartEnableEXT(cmd, VK_FALSE);
			vkCmdSetRasterizationSamplesEXT(cmd, VK_SAMPLE_COUNT_1_BIT);
		}
		{
			const VkSampleMask sample_mask = 0x1;
			vkCmdSetSampleMaskEXT(cmd, VK_SAMPLE_COUNT_1_BIT, &sample_mask);
		}
		{
			vkCmdSetAlphaToCoverageEnableEXT(cmd, VK_FALSE);
			vkCmdSetCullModeEXT(cmd, VK_TRUE);
			vkCmdSetPolygonModeEXT(cmd, VK_POLYGON_MODE_FILL);
		}
		{
			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)swapChainExtent.width;
			viewport.height = (float)swapChainExtent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(cmd, 0, 1, &viewport);
			vkCmdSetViewportWithCountEXT(cmd, 1, &viewport);
		}
		{
			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = swapChainExtent;
			vkCmdSetScissor(cmd, 0, 1, &scissor);
			vkCmdSetScissorWithCountEXT(cmd, 1, &scissor);
		}
		{
			vkCmdSetFrontFaceEXT(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
			vkCmdSetDepthTestEnableEXT(cmd, VK_FALSE);
			vkCmdSetDepthWriteEnableEXT(cmd, VK_FALSE);
			vkCmdSetDepthCompareOpEXT(cmd, VK_COMPARE_OP_GREATER);
			vkCmdSetDepthBoundsTestEnableEXT(cmd, VK_FALSE);
			vkCmdSetDepthBiasEnableEXT(cmd, VK_FALSE);
			vkCmdSetStencilTestEnableEXT(cmd, VK_FALSE);
			vkCmdSetLogicOpEnableEXT(cmd, VK_FALSE);
		}
		{
			VkBool32 color_blend_enables[] = { VK_FALSE };
			vkCmdSetColorBlendEnableEXT(cmd, 0, 1, color_blend_enables);
		}
		{
			VkColorComponentFlags color_component_flags[] = { VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_A_BIT };
			vkCmdSetColorWriteMaskEXT(cmd, 0, 1, color_component_flags);
		}
		{
			vkCmdSetVertexInputEXT(cmd, 0, nullptr, 0, nullptr);
		}
	}

	{
		// Clear image
		transitionImage(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		VkClearColorValue clearValue;
		clearValue = { { 0.0f, 0.05f, 0.1f, 1.0f } };
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &subresourceRange);
	}
	{
		// Start dynamic render pass
		transitionImage(cmd, image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		{
			VkRenderingAttachmentInfo colorAttachment = {};
			colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			colorAttachment.pNext = nullptr;
			colorAttachment.imageView = imageView;
			colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

			VkRect2D render_area = VkRect2D{ VkOffset2D{ 0, 0 }, m_swapChainExtent };

			VkRenderingInfo renderInfo = {};
			renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			renderInfo.pNext = nullptr;
			renderInfo.renderArea = render_area;
			renderInfo.layerCount = 1;
			renderInfo.colorAttachmentCount = 1;
			renderInfo.pColorAttachments = &colorAttachment;
			renderInfo.pDepthAttachment = nullptr;
			renderInfo.pStencilAttachment = nullptr;
			vkCmdBeginRendering(cmd, &renderInfo);
		}

	}
	{
		// triangle
		const VkDebugUtilsLabelEXT triangleLabel =
		{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
			.pNext = NULL,
			.pLabelName = "triangleLabel",
			.color = { 1.0f, 0.0f, 0.0f, 1.0f },
		};
		vkCmdBeginDebugUtilsLabelEXT(cmd, &triangleLabel);
		const VkShaderStageFlagBits stages[2] =
		{
			VK_SHADER_STAGE_VERTEX_BIT,
			VK_SHADER_STAGE_FRAGMENT_BIT
		};
		vkCmdBindShadersEXT(cmd, 2, stages, m_shaders.data());
		const VkShaderStageFlagBits unusedStages[3] =
		{
			VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
			VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
			VK_SHADER_STAGE_GEOMETRY_BIT
		};
		vkCmdBindShadersEXT(cmd, 3, unusedStages, NULL);
		vkCmdDraw(cmd, 3, 1, 0, 0);
		vkCmdEndDebugUtilsLabelEXT(cmd);
	}

	{
		// end rendering, transition image and submit for presentation
		vkCmdEndRendering(cmd);
		{
			transitionImage(cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			result = vkEndCommandBuffer(cmd);
			if (result != VK_SUCCESS) return result;
		}

		{
			// submit recorded commands to the queue
			VkCommandBufferSubmitInfo cmdBufferSubmitInfo = {};
			cmdBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
			cmdBufferSubmitInfo.pNext = nullptr;
			cmdBufferSubmitInfo.commandBuffer = cmd;
			cmdBufferSubmitInfo.deviceMask = 0;

			VkSemaphoreSubmitInfo waitInfo = {};
			waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
			waitInfo.pNext = nullptr;
			waitInfo.semaphore = imageAvailableSignal;
			waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
			waitInfo.deviceIndex = 0;
			waitInfo.value = 1;

			VkSemaphoreSubmitInfo signalInfo = {};
			signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
			signalInfo.pNext = nullptr;
			signalInfo.semaphore = renderedFinisishedSignal;
			signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
			signalInfo.deviceIndex = 0;
			signalInfo.value = 1;

			VkSubmitInfo2 submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
			submitInfo.pNext = nullptr;
			submitInfo.waitSemaphoreInfoCount = 1;
			submitInfo.pWaitSemaphoreInfos = &waitInfo;
			submitInfo.signalSemaphoreInfoCount = 1;
			submitInfo.pSignalSemaphoreInfos = &signalInfo;
			submitInfo.commandBufferInfoCount = 1;
			submitInfo.pCommandBufferInfos = &cmdBufferSubmitInfo;
			result = vkQueueSubmit2(m_presentQueue.value(), 1, &submitInfo, fence);
			if (result != VK_SUCCESS) return result;
		}
		{
			// Queue image for presentation
			VkSwapchainKHR swapChains[] = { m_swapchain.value() };
			VkPresentInfoKHR presentInfo = {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = &renderedFinisishedSignal;
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = swapChains;
			presentInfo.pImageIndices = &imageIndex;

			result = vkQueuePresentKHR(m_presentQueue.value(), &presentInfo);
			if (result != VK_SUCCESS) return result;
		}
	}



	m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	return VK_SUCCESS;
}

Rhi::~Rhi() {
	if (m_device.has_value()) {
		vkDeviceWaitIdle(m_device.value());
	}
	for (VkFence fence : m_inFlightFence) {
		vkDestroyFence(m_device.value(), fence, nullptr);
	}
	for (VkSemaphore semaphore : m_imageAvailableSemaphores) {
		vkDestroySemaphore(m_device.value(), semaphore, nullptr);
	}
	for (VkSemaphore semaphore : m_renderFinishedSemaphores) {
		vkDestroySemaphore(m_device.value(), semaphore, nullptr);
	}
	if (m_commandPool.has_value()) {
		vkDestroyCommandPool(m_device.value(), m_commandPool.value(), nullptr);
	}
	for (VkShaderEXT shader : m_shaders) {
		vkDestroyShaderEXT(m_device.value(), shader, nullptr);
	}
	for (VkImageView imageView : m_swapChainImageViews) {
		vkDestroyImageView(m_device.value(), imageView, nullptr);
	}
	if (m_swapchain.has_value()) {
		vkDestroySwapchainKHR(m_device.value(), m_swapchain.value(), nullptr);
	}
	if (m_debugMessenger.has_value()) {
		vkDestroyDebugUtilsMessengerEXT(m_instance.value(), m_debugMessenger.value(), nullptr);
	}
	if (m_allocator.has_value()) {
		vmaDestroyAllocator(m_allocator.value());
	}
	if (m_device.has_value()) {
		VkResult result = vkDeviceWaitIdle(m_device.value());
		if (result == VK_SUCCESS) vkDestroyDevice(m_device.value(), NULL);
	}
	if (m_surface.has_value()) {
		SDL_Vulkan_DestroySurface(m_instance.value(), m_surface.value(), nullptr);
	}
	if (m_instance.has_value()) {
		vkDestroyInstance(m_instance.value(), NULL);
	}
}
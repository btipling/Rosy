#define VK_USE_PLATFORM_WIN32_KHR
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
	result = this->initDrawImage();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to init draw image! %d\n", result);
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
	result = this->initUI(window);
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to init UI! %d\n", result);
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

VkResult Rhi::initDrawImage() {
	VkResult result;

	VkExtent3D drawImageExtent = {
		m_swapChainExtent.width,
		m_swapChainExtent.height,
		1
	};
	AllocatedImage drawImage = {};
	drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	drawImage.imageExtent = drawImageExtent;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo rimg_info = imgCreateInfo(drawImage.imageFormat, drawImageUsages, drawImageExtent);

	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(m_allocator.value(), &rimg_info, &rimg_allocinfo, &drawImage.image, &drawImage.allocation, nullptr);

	VkImageViewCreateInfo rview_info = imgViewCreateInfo(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	result = vkCreateImageView(m_device.value(), &rview_info, nullptr, &drawImage.imageView);
	if (result != VK_SUCCESS) return result;
	m_drawImage = drawImage;
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
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

Rhi::Rhi(rosy_config::Config cfg) :m_cfg{ cfg }, m_requiredFeatures{ requiredFeatures } {
	memset(&m_requiredFeatures, 0, sizeof(VkPhysicalDeviceFeatures));
}

Rhi::~Rhi() {
	deinit();
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
	result = this->initCommands();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to init commands! %d\n", result);
		return result;
	}
	result = this->initDefaultData();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to init default data! %d\n", result);
		return result;
	}
	return VK_SUCCESS;
}

void Rhi::deinit() {
	if (m_deinited) return;
	m_deinited = true;
	{
		// Wait for everything to be done.
		if (m_device.has_value()) {
			vkDeviceWaitIdle(m_device.value());
		}
	}

	// Deinit begin in the reverse order from how it was created.
	deinitUI();

	if (m_rectangle.has_value()) {
		GPUMeshBuffers rectangle = m_rectangle.value();
		destroyBuffer(rectangle.vertexBuffer);
		destroyBuffer(rectangle.indexBuffer);
	}
	if (m_immFence.has_value()) {
		vkDestroyFence(m_device.value(), m_immFence.value(), nullptr);
	}
	if (m_immCommandPool.has_value()) {
		vkDestroyCommandPool(m_device.value(), m_immCommandPool.value(), nullptr);
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
	if (m_shaderPL.has_value()) {
		vkDestroyPipelineLayout(m_device.value(), m_shaderPL.value(), nullptr);
	}
	for (VkShaderEXT shader : m_shaders) {
		vkDestroyShaderEXT(m_device.value(), shader, nullptr);
	}
	for (VkImageView imageView : m_swapChainImageViews) {
		vkDestroyImageView(m_device.value(), imageView, nullptr);
	}
	if (m_drawImage.has_value()) {
		AllocatedImage drawImage = m_drawImage.value();
		vkDestroyImageView(m_device.value(), drawImage.imageView, nullptr);
		vmaDestroyImage(m_allocator.value(), drawImage.image, drawImage.allocation);
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
	allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

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
	{
		VkFence fence;
		result = vkCreateFence(device, &fenceInfo, nullptr, &fence);
		m_immFence = fence;
	}
	return VK_SUCCESS;
}


VkResult Rhi::initCommands() {
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

VkResult Rhi::initDefaultData() {
		std::array<Vertex, 4> rectVertices;

		rectVertices[0].position = { 0.5f ,-0.5f, 0.0f, 1.0f };
		rectVertices[1].position = { 0.5f, 0.5f, 0.0f, 1.0f };
		rectVertices[2].position = { -0.5f, -0.5f, 0.0f, 1.0f };
		rectVertices[3].position = { -0.5f, 0.5f, 0.0f, 1.0f };

		rectVertices[0].textureCoordinates = { 0.0f, 0.0f, 0.0f, 0.0f };
		rectVertices[1].textureCoordinates = { 0.0f, 0.0f, 0.0f, 0.0f };
		rectVertices[2].textureCoordinates = { 0.0f, 0.0f, 0.0f, 0.0f };
		rectVertices[3].textureCoordinates = { 0.0f, 0.0f, 0.0f, 0.0f };

		rectVertices[0].normal = { 0.0f, 0.0f, 0.0f, 0.0f };
		rectVertices[1].normal = { 0.0f, 0.0f, 0.0f, 0.0f };
		rectVertices[2].normal = { 0.0f, 0.0f, 0.0f, 0.0f };
		rectVertices[3].normal = { 0.0f, 0.0f, 0.0f, 0.0f };


		rectVertices[0].color = { 1.0f, 1.0f, 0.0f, 1.0f };
		rectVertices[1].color = { 0.5f, 0.5f, 0.5f, 1.0f };
		rectVertices[2].color = { 1.0f, 0.0f, 0.0f, 1.0f };
		rectVertices[3].color = { 0.0f, 1.0f, 0.0f, 1.0f };

		std::array<uint32_t, 6> rectIndices;

		rectIndices[0] = 0;
		rectIndices[1] = 1;
		rectIndices[2] = 2;

		rectIndices[3] = 2;
		rectIndices[4] = 1;
		rectIndices[5] = 3;

		GPUMeshBuffersResult rectangleResult = uploadMesh(rectIndices, rectVertices);
		if (rectangleResult.result != VK_SUCCESS) return rectangleResult.result;
		m_rectangle = rectangleResult.buffers;
		auto result = loadGltfMeshes(this, "..\\assets\\basicmesh.glb");
		if (result.has_value()) {
			m_testMeshes = result.value();
		}
		return VK_SUCCESS;
}

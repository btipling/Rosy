#include "RHI.h"
#include <Windows.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <strsafe.h>
#include "../config/Config.h"
#include "../utils/utils.h"

static const char* instanceLayers[] = {
	"VK_LAYER_LUNARG_api_dump",
	"VK_LAYER_KHRONOS_validation",
	//"VK_LAYER_LUNARG_monitor",
	//"VK_LAYER_KHRONOS_profiles",
	"VK_LAYER_LUNARG_crash_diagnostic",
	"VK_LAYER_KHRONOS_shader_object",
	"VK_LAYER_KHRONOS_synchronization2",
};
static VkPhysicalDeviceFeatures requiredFeatures;

Rhi::Rhi(rosy_config::Config cfg) :m_cfg{ cfg }, m_requiredFeatures{ requiredFeatures } {
	memset(&m_requiredFeatures, 0, sizeof(VkPhysicalDeviceFeatures));
}

void Rhi::init() {

	VkResult result;
	result = this->queryInstanceLayers();
	if (result != VK_SUCCESS) {
		return;
	}
	result = this->initInstance();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to create Vulkan instance! %d\n", result);
		return;
	}
	result = this->initPhysicalDevice();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to create Vulkan physical device! %d\n", result);
		return;
	}
	result = this->initDevice();
	if (result != VK_SUCCESS) {
		rosy_utils::DebugPrintW(L"Failed to create Vulkan device! %d\n", result);
		return;
	}

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
	for (VkLayerProperties lp : layers) {
		rosy_utils::DebugPrintA("Layer name: %s layer description: %s\n", lp.layerName, lp.description);
		for (const char* layerName : instanceLayers) {
			if (strcmp(layerName, lp.layerName)) {
				m_instanceLayerProperties.push_back(layerName);
			}
		}
	}
	return result;
}

VkResult Rhi::initInstance() {
	uint32_t extensionCount;
	auto extensionNames = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Rosy";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Rosy";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledLayerCount = m_instanceLayerProperties.size();
	createInfo.ppEnabledLayerNames = m_instanceLayerProperties.data();
	createInfo.enabledExtensionCount = extensionCount;
	createInfo.ppEnabledExtensionNames = extensionNames;

	VkInstance instance;
	VkResult result = vkCreateInstance(&createInfo, NULL, &instance);
	if (result != VK_SUCCESS) return result;
	m_instance = instance;
	return result;
}

VkResult Rhi::initPhysicalDevice() {
	if (!m_instance.has_value()) return VK_NOT_READY;
	std::vector<VkPhysicalDevice> physicalDevices;
	OutputDebugStringW(L"Vulkan instance created successfully!\n");

	uint32_t physicalDeviceCount = 0;
	VkResult result = vkEnumeratePhysicalDevices(m_instance.value(), &physicalDeviceCount, nullptr);

	physicalDevices.resize(physicalDeviceCount);
	vkEnumeratePhysicalDevices(m_instance.value(), &physicalDeviceCount, &physicalDevices[0]);
	for (const VkPhysicalDevice& p_device : physicalDevices) {
		// get device properties
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(p_device, &deviceProperties);
		if (deviceProperties.vendorID == m_cfg.device_vendor) {
			{
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
	for (std::uint32_t i = 0; i < queueFamilyPropertiesData.size(); ++i) {
		VkQueueFamilyProperties qfmp = queueFamilyPropertiesData[i];
		if (qfmp.timestampValidBits < 64) continue;
		if (!(qfmp.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT))) continue;
		if (qfmp.queueCount > queueCount) {
			queueIndex = i;
			queueCount = qfmp.queueCount;
		}
	}
	m_queueIndex = queueIndex;
	m_queueCount = queueCount;
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
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = nullptr;
	deviceCreateInfo.flags = 0;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
	deviceCreateInfo.enabledLayerCount = 0;
	deviceCreateInfo.ppEnabledLayerNames = nullptr;
	deviceCreateInfo.enabledExtensionCount = 0;
	deviceCreateInfo.ppEnabledExtensionNames = nullptr;
	deviceCreateInfo.pEnabledFeatures = &m_requiredFeatures;
	VkDevice device;
	VkResult result = vkCreateDevice(m_physicalDevice.value(), &deviceCreateInfo, nullptr, &device);
	if (result != VK_SUCCESS) return result;

	rosy_utils::DebugPrintW(L"Vulkan device created successfully!\n");
	m_device = device;
	return result;
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

Rhi::~Rhi() {
	if (m_device.has_value()) {
		vkDestroyDevice(m_device.value(), NULL);
	}
	if (m_instance.has_value()) {
		vkDestroyInstance(m_instance.value(), NULL);
	}
}
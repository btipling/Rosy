#include "RHI.h"
#include <Windows.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <strsafe.h>
#include "../config/Config.h"
#include "../utils/utils.h"


RhiInitResult RhiInit(rosy_config::Config cfg) {

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
	createInfo.enabledLayerCount = 0;
	createInfo.ppEnabledLayerNames = NULL;
	createInfo.enabledExtensionCount = extensionCount;
	createInfo.ppEnabledExtensionNames = extensionNames;

	VkInstance instance;
	VkResult result = vkCreateInstance(&createInfo, NULL, &instance);
	std::optional<VkPhysicalDevice> physicalDevice;

	std::vector<VkPhysicalDevice> physicalDevices;
	std::optional<VkPhysicalDeviceProperties> physicalDeviceProperties = std::nullopt;
	std::optional<VkPhysicalDeviceFeatures> physicalDeviceFeatures = std::nullopt;
	std::optional<VkPhysicalDeviceMemoryProperties> physicalDeviceMemoryProperties = std::nullopt;
	std::optional<std::vector<VkQueueFamilyProperties>> queueFamilyProperties = std::nullopt;

	if (result == VK_SUCCESS) {
		OutputDebugStringW(L"Vulkan instance created successfully!\n");
		uint32_t physicalDeviceCount = 0;
		result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
		if (result == VK_SUCCESS) {
			physicalDevices.resize(physicalDeviceCount);
			vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, &physicalDevices[0]);
			for (const VkPhysicalDevice& device : physicalDevices) {
				// get device properties
				VkPhysicalDeviceProperties deviceProperties;
				vkGetPhysicalDeviceProperties(device, &deviceProperties);
				if (deviceProperties.vendorID == cfg.device_vendor) {
					{
						physicalDevice = device;
						physicalDeviceProperties = deviceProperties;
					}
					{
						// features
						VkPhysicalDeviceFeatures features;
						vkGetPhysicalDeviceFeatures(device, &features);
						physicalDeviceFeatures = features;
					}
					{
						// memory
						VkPhysicalDeviceMemoryProperties memProps;
						vkGetPhysicalDeviceMemoryProperties(device, &memProps);
						physicalDeviceMemoryProperties = memProps;
					}
					{
						// queues
						uint32_t queueCount = 0;
						vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);
						std::vector<VkQueueFamilyProperties> queueFamilyPropertiesData;
						queueFamilyPropertiesData.resize(queueCount);
						vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, &queueFamilyPropertiesData[0]);
						queueFamilyProperties = queueFamilyPropertiesData;
					}
				}

			}
		}
	}
	else {
		rosy_utils::DebugPrintW(L"Failed to create Vulkan instance!\n");
	}
	uint32_t queueCount = 0;
	uint32_t queueIndex = 0;
	if (physicalDevice.has_value()) {
		std::vector<VkQueueFamilyProperties> queueFamilyPropertiesData = queueFamilyProperties.value();
		for (std::size_t i = 0; i < queueFamilyPropertiesData.size(); ++i) {
			VkQueueFamilyProperties qfmp = queueFamilyPropertiesData[i];
			if (qfmp.timestampValidBits < 64) continue;
			if (!(qfmp.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT))) continue;
			if (qfmp.queueCount > queueCount) {
				queueIndex = i;
				queueCount = qfmp.queueCount;
			}
		}
	}

	struct RhiInitResult res = RhiInitResult{
		.result = result,
		.instance = instance,
		.physicalDevice = physicalDevice,
		.physicalDeviceProperties = physicalDeviceProperties,
		.physicalDeviceFeatures = physicalDeviceFeatures,
		.physicalDeviceMemoryProperties = physicalDeviceMemoryProperties,
		.queueFamilyProperties = queueFamilyProperties,
		.queueIndex = queueIndex,
		.queueCount = queueCount,
	};
	return res;
}
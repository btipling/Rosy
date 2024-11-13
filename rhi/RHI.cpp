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
	std::optional<VkPhysicalDeviceFeatures> supportedFeatures = std::nullopt;
	std::optional<VkPhysicalDeviceMemoryProperties> physicalDeviceMemoryProperties = std::nullopt;
	std::optional<std::vector<VkQueueFamilyProperties>> queueFamilyProperties = std::nullopt;
	std::optional<VkDevice> device;

	if (result == VK_SUCCESS) {
		OutputDebugStringW(L"Vulkan instance created successfully!\n");
		uint32_t physicalDeviceCount = 0;
		result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
		if (result == VK_SUCCESS) {
			physicalDevices.resize(physicalDeviceCount);
			vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, &physicalDevices[0]);
			for (const VkPhysicalDevice& p_device : physicalDevices) {
				// get device properties
				VkPhysicalDeviceProperties deviceProperties;
				vkGetPhysicalDeviceProperties(p_device, &deviceProperties);
				if (deviceProperties.vendorID == cfg.device_vendor) {
					{
						physicalDevice = p_device;
						physicalDeviceProperties = deviceProperties;
					}
					{
						// features
						VkPhysicalDeviceFeatures features;
						vkGetPhysicalDeviceFeatures(p_device, &features);
						supportedFeatures = features;
					}
					{
						// memory
						VkPhysicalDeviceMemoryProperties memProps;
						vkGetPhysicalDeviceMemoryProperties(p_device, &memProps);
						physicalDeviceMemoryProperties = memProps;
					}
					{
						// queues
						uint32_t queueCount = 0;
						vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queueCount, nullptr);
						std::vector<VkQueueFamilyProperties> queueFamilyPropertiesData;
						queueFamilyPropertiesData.resize(queueCount);
						vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queueCount, &queueFamilyPropertiesData[0]);
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
	VkPhysicalDeviceFeatures requiredFeatures;
	memset(&requiredFeatures, 0, sizeof(VkPhysicalDeviceFeatures));
	VkPhysicalDeviceFeatures supportedFeaturesData = supportedFeatures.value();
	requiredFeatures.multiDrawIndirect = supportedFeaturesData.multiDrawIndirect;
	requiredFeatures.tessellationShader = supportedFeaturesData.tessellationShader;
	requiredFeatures.geometryShader = supportedFeaturesData.geometryShader;
	if (physicalDevice.has_value()) {
		VkPhysicalDevice p_device = physicalDevice.value();
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
		VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
		deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		deviceQueueCreateInfo.pNext = nullptr;
		deviceQueueCreateInfo.flags = 0;
		deviceQueueCreateInfo.queueFamilyIndex = queueIndex;
		deviceQueueCreateInfo.queueCount = queueCount;
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
		deviceCreateInfo.pEnabledFeatures = &requiredFeatures;
		VkDevice n_device;
		VkResult cd_result = vkCreateDevice(p_device, &deviceCreateInfo, nullptr, &n_device);
		if (cd_result == VK_SUCCESS) {
			rosy_utils::DebugPrintW(L"Vulkan device created successfully!\n");
			device = n_device;
		}
		else {
			rosy_utils::DebugPrintW(L"Failed to create Vulkan device! %d\n", cd_result);
		}

	}

	struct RhiInitResult res = RhiInitResult{
		.result = result,
		.instance = instance,
		.physicalDevice = physicalDevice,
		.physicalDeviceProperties = physicalDeviceProperties,
		.supportedFeatures = supportedFeatures,
		.physicalDeviceMemoryProperties = physicalDeviceMemoryProperties,
		.queueFamilyProperties = queueFamilyProperties,
		.queueIndex = queueIndex,
		.queueCount = queueCount,
		.requiredFeatures = requiredFeatures,
		.device = device,
	};
	return res;
}
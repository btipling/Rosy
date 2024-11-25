#pragma once

#include "../Rosy.h"

struct DescriptorSetLayoutResult {
	VkResult result;
	VkDescriptorSetLayout set;
};

struct DescriptorSetResult {
	VkResult result;
	VkDescriptorSet set;
};

struct DescriptorLayoutBuilder {

	std::vector<VkDescriptorSetLayoutBinding> bindings;

	void addBinding(uint32_t binding, VkDescriptorType type);
	void clear();
	DescriptorSetLayoutResult build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct DescriptorAllocator {

	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	VkDescriptorPool pool;

	void initPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
	void clearDescriptors(VkDevice device);
	void destroyPool(VkDevice device);

	DescriptorSetResult allocate(VkDevice device, VkDescriptorSetLayout layout);
};
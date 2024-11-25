#include "RHI.h"

void DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type) {
	VkDescriptorSetLayoutBinding newbind = {};
	newbind.binding = binding;
	newbind.descriptorCount = 1;
	newbind.descriptorType = type;

	bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear() {
	bindings.clear();
}

DescriptorSetLayoutResult DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags) {
	for (auto& b : bindings) {
		b.stageFlags |= shaderStages;
	}

	VkDescriptorSetLayoutCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.pNext = pNext;
	info.pBindings = bindings.data();
	info.bindingCount = (uint32_t)bindings.size();
	info.flags = flags;

	VkDescriptorSetLayout set;
	VkResult result;
	result = vkCreateDescriptorSetLayout(device, &info, nullptr, &set);
	if (result != VK_SUCCESS) {
		DescriptorSetLayoutResult rv = {};
		rv.result = result;
		return rv;
	}
	{
		DescriptorSetLayoutResult rv = {};
		rv.result = VK_SUCCESS;
		rv.set = set;
		return rv;
	}
}

void DescriptorAllocator::initPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) {
	std::vector<VkDescriptorPoolSize> poolSizes;

	for (PoolSizeRatio ratio : poolRatios) {
		poolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = uint32_t(ratio.ratio * maxSets)
			});
	}

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = 0;
	poolInfo.maxSets = maxSets;
	poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();

	vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool);
}

void DescriptorAllocator::clearDescriptors(VkDevice device) {
	vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroyPool(VkDevice device) {
	vkDestroyDescriptorPool(device, pool, nullptr);
}

DescriptorSetResult DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout) {
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.descriptorPool = pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	VkResult result;
	VkDescriptorSet set;
	result = vkAllocateDescriptorSets(device, &allocInfo, &set);
	if (result != VK_SUCCESS) {
		DescriptorSetResult rv = {};
		rv.result = result;
		return rv;
	}
	{
		DescriptorSetResult rv = {};
		rv.result = VK_SUCCESS;
		rv.set = set;
		return rv;
	}
}
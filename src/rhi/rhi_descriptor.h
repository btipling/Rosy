#pragma once

#include "../Rosy.h"

struct descriptor_set_layout_result {
	VkResult result;
	VkDescriptorSetLayout set;
};

struct descriptor_set_result {
	VkResult result;
	VkDescriptorSet set;
};

struct descriptor_layout_builder {

	std::vector<VkDescriptorSetLayoutBinding> bindings;

	void add_binding(uint32_t binding, VkDescriptorType type);
	void clear();
	descriptor_set_layout_result build(VkDevice device, VkShaderStageFlags shader_stages, void* p_next = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct descriptor_allocator {

	struct pool_size_ratio {
		VkDescriptorType type;
		float ratio;
	};

	VkDescriptorPool pool;

	void init_pool(VkDevice device, uint32_t max_sets, std::span<pool_size_ratio> pool_ratios);
	void clear_descriptors(VkDevice device);
	void destroy_pool(VkDevice device);

	descriptor_set_result allocate(VkDevice device, VkDescriptorSetLayout layout) const;
};

struct descriptor_allocator_growable {
public:
	struct pool_size_ratio {
		VkDescriptorType type;
		float ratio;
	};

	void init(VkDevice device, uint32_t max_sets, std::span<pool_size_ratio> pool_ratios);
	void clear_pools(VkDevice device);
	void destroy_pools(VkDevice device);

	descriptor_set_result allocate(VkDevice device, VkDescriptorSetLayout layout, void* p_next = nullptr);
private:
	VkDescriptorPool get_pool(VkDevice device);
	static VkDescriptorPool create_pool(VkDevice device, uint32_t set_count, std::span<pool_size_ratio> pool_ratios);

	std::vector<pool_size_ratio> ratios_;
	std::vector<VkDescriptorPool> full_pools_;
	std::vector<VkDescriptorPool> ready_pools_;
	uint32_t sets_per_pool_ = 0;

};
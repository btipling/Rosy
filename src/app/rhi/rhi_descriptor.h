#pragma once

#include "../../Rosy.h"
#include "../../rhi/descriptor.h"

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
	descriptor_set_layout_result build(VkDevice device, VkShaderStageFlags shader_stages, const void* p_next = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct descriptor_allocator {

	struct pool_size_ratio {
		VkDescriptorType type;
		float ratio;
	};

	VkDescriptorPool pool;

	void init_pool(VkDevice device, uint32_t max_sets, std::span<pool_size_ratio> pool_ratios);
	void clear_descriptors(VkDevice device) const;
	void destroy_pool(VkDevice device) const;

	descriptor_set_result allocate(VkDevice device, VkDescriptorSetLayout layout) const;
};

class descriptor_allocator_growable {
public:
	struct pool_size_ratio {
		VkDescriptorType type;
		float ratio;
	};

	void init(VkDevice device, uint32_t max_sets, std::span<pool_size_ratio> pool_ratios);
	void clear_pools(VkDevice device);
	void destroy_pools(VkDevice device);

	descriptor_set_result allocate(VkDevice device, VkDescriptorSetLayout layout, const void* p_next = nullptr);
private:
	VkDescriptorPool get_pool(VkDevice device);
	static VkDescriptorPool create_pool(VkDevice device, uint32_t set_count, std::span<pool_size_ratio> pool_ratios);

	std::vector<pool_size_ratio> ratios_;
	std::vector<VkDescriptorPool> full_pools_;
	std::vector<VkDescriptorPool> ready_pools_;
	uint32_t sets_per_pool_ = 0;

};

struct descriptor_writer {
	std::deque<VkDescriptorImageInfo> image_infos;
	std::deque<VkDescriptorBufferInfo> buffer_infos;
	std::vector<VkWriteDescriptorSet> writes;

	void write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
	void write_sampled_image(int binding, VkImageView image, VkImageLayout layout, VkDescriptorType type);
	void write_sampler(int binding, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
	void write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);

	void clear();
	void update_set(VkDevice device, VkDescriptorSet set);
};

constexpr uint32_t descriptor_storage_image_binding{ 2 };
constexpr uint32_t descriptor_sampled_image_binding{ 0 };
constexpr uint32_t descriptor_sample_binding{ 1 };

constexpr uint32_t descriptor_max_storage_image_descriptors{ 100'000 };
constexpr uint32_t descriptor_max_sampled_image_descriptors{ 100'000 };
constexpr uint32_t descriptor_max_sample_descriptors{ 1000 };

class descriptor_sets_manager
{
public:
	descriptor::set storage_images;
	descriptor::set sampled_images;
	descriptor::set samples;
	explicit descriptor_sets_manager();
	VkResult init(VkDevice device);
	void deinit(VkDevice device);
private:
	std::optional<VkDescriptorPool> descriptor_pool_;
	std::optional<VkDescriptorSetLayout> descriptor_set_layout_;
	std::optional<VkDescriptorSet> descriptor_set_;
};
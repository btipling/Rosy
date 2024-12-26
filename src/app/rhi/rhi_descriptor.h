#pragma once

#include "../../Rosy.h"
#include "../../rhi/descriptor.h"

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
	std::optional<VkDescriptorSetLayout> descriptor_set_layout;
	std::optional<VkDescriptorSet> descriptor_set;
	explicit descriptor_sets_manager();
	VkResult init_pool(VkDevice device);
	VkResult init_sets(VkDevice device);
	VkResult reset(VkDevice device);
	void deinit(VkDevice device);
	uint32_t write_sampled_image(VkDevice device, const VkImageView image, const VkImageLayout layout);
	uint32_t write_sampler(VkDevice device, const VkSampler sampler, const VkImageLayout layout);
private:
	std::optional<VkDescriptorPool> descriptor_pool_;
	std::vector<VkDescriptorPoolSize> pool_sizes_;
};
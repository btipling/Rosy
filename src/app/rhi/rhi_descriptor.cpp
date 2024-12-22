#include "rhi.h"

void descriptor_layout_builder::add_binding(const uint32_t binding, const VkDescriptorType type)
{
	VkDescriptorSetLayoutBinding new_bind{};
	new_bind.binding = binding;
	new_bind.descriptorCount = 1;
	new_bind.descriptorType = type;

	bindings.push_back(new_bind);
}

void descriptor_layout_builder::clear()
{
	bindings.clear();
}

descriptor_set_layout_result descriptor_layout_builder::build(const VkDevice device, const VkShaderStageFlags shader_stages,
                                                              const void* p_next, const VkDescriptorSetLayoutCreateFlags flags)
{
	for (auto& b : bindings)
	{
		b.stageFlags |= shader_stages;
	}

	VkDescriptorSetLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.pNext = p_next;
	info.pBindings = bindings.data();
	info.bindingCount = static_cast<uint32_t>(bindings.size());
	info.flags = flags;

	VkDescriptorSetLayout set{};
	if (const VkResult result = vkCreateDescriptorSetLayout(device, &info, nullptr, &set); result != VK_SUCCESS)
	{
		descriptor_set_layout_result rv{};
		rv.result = result;
		return rv;
	}
	{
		descriptor_set_layout_result rv{};
		rv.result = VK_SUCCESS;
		rv.set = set;
		return rv;
	}
}

void descriptor_allocator::init_pool(const VkDevice device, const uint32_t max_sets, const std::span<pool_size_ratio> pool_ratios)
{
	std::vector<VkDescriptorPoolSize> pool_sizes{};

	for (auto [type, ratio] : pool_ratios)
	{
		pool_sizes.push_back(VkDescriptorPoolSize{
			.type = type,
			.descriptorCount = static_cast<uint32_t>(ratio * max_sets)
		});
	}

	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = 0;
	pool_info.maxSets = max_sets;
	pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
	pool_info.pPoolSizes = pool_sizes.data();

	vkCreateDescriptorPool(device, &pool_info, nullptr, &pool);
}

void descriptor_allocator::clear_descriptors(const VkDevice device) const
{
	vkResetDescriptorPool(device, pool, 0);
}

void descriptor_allocator::destroy_pool(const VkDevice device) const
{
	vkDestroyDescriptorPool(device, pool, nullptr);
}

descriptor_set_result descriptor_allocator::allocate(const VkDevice device, const VkDescriptorSetLayout layout) const
{
	VkDescriptorSetAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.pNext = nullptr;
	alloc_info.descriptorPool = pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &layout;

	VkDescriptorSet set{};
	if (const VkResult result = vkAllocateDescriptorSets(device, &alloc_info, &set); result != VK_SUCCESS)
	{
		descriptor_set_result rv{};
		rv.result = result;
		return rv;
	}
	{
		descriptor_set_result rv{};
		rv.result = VK_SUCCESS;
		rv.set = set;
		return rv;
	}
}

void descriptor_allocator_growable::init(const VkDevice device, const uint32_t max_sets,
                                         const std::span<pool_size_ratio> pool_ratios)
{
	ratios_.clear();

	for (auto r : pool_ratios)
	{
		ratios_.push_back(r);
	}

	const VkDescriptorPool new_pool = create_pool(device, max_sets, pool_ratios);

	sets_per_pool_ = max_sets * 1.5;

	ready_pools_.push_back(new_pool);
}

void descriptor_allocator_growable::clear_pools(const VkDevice device)
{
	for (const auto p : ready_pools_)
	{
		vkResetDescriptorPool(device, p, 0);
	}
	for (auto p : full_pools_)
	{
		vkResetDescriptorPool(device, p, 0);
		ready_pools_.push_back(p);
	}
	full_pools_.clear();
}

void descriptor_allocator_growable::destroy_pools(const VkDevice device)
{
	for (const auto p : ready_pools_)
	{
		vkDestroyDescriptorPool(device, p, nullptr);
	}
	ready_pools_.clear();
	for (const auto p : full_pools_)
	{
		vkDestroyDescriptorPool(device, p, nullptr);
	}
	full_pools_.clear();
}

descriptor_set_result descriptor_allocator_growable::allocate(const VkDevice device, const VkDescriptorSetLayout layout,
                                                              const void* p_next)
{
	VkDescriptorPool pool_to_use = get_pool(device);

	VkDescriptorSetAllocateInfo alloc_info{};
	alloc_info.pNext = p_next;
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = pool_to_use;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &layout;

	VkDescriptorSet ds;
	VkResult result = vkAllocateDescriptorSets(device, &alloc_info, &ds);
	if (result == VK_SUCCESS)
	{
		ready_pools_.push_back(pool_to_use);
		descriptor_set_result rv{};
		rv.result = VK_SUCCESS;
		rv.set = ds;
		return rv;
	}
	// try again and get a pool
	if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
	{
		full_pools_.push_back(pool_to_use);

		pool_to_use = get_pool(device);
		alloc_info.descriptorPool = pool_to_use;

		result = vkAllocateDescriptorSets(device, &alloc_info, &ds);
		if (result == VK_SUCCESS)
		{
			ready_pools_.push_back(pool_to_use);
			descriptor_set_result rv{};
			rv.result = VK_SUCCESS;
			rv.set = ds;
			return rv;
		}
	}
	{
		descriptor_set_result rv{};
		rv.result = result;
		return rv;
	}
}

VkDescriptorPool descriptor_allocator_growable::get_pool(const VkDevice device)
{
	VkDescriptorPool new_pool;
	if (ready_pools_.size() != 0)
	{
		new_pool = ready_pools_.back();
		ready_pools_.pop_back();
	}
	else
	{
		new_pool = create_pool(device, sets_per_pool_, ratios_);

		sets_per_pool_ = sets_per_pool_ * 1.5;
		sets_per_pool_ = std::min<uint32_t>(sets_per_pool_, 4092);
	}

	return new_pool;
}

VkDescriptorPool descriptor_allocator_growable::create_pool(const VkDevice device, const uint32_t set_count,
                                                            const std::span<pool_size_ratio> pool_ratios)
{
	std::vector<VkDescriptorPoolSize> pool_sizes;
	for (auto [type, ratio] : pool_ratios)
	{
		pool_sizes.push_back(VkDescriptorPoolSize{
			.type = type,
			.descriptorCount = static_cast<uint32_t>(ratio * set_count)
		});
	}

	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = 0;
	pool_info.maxSets = set_count;
	pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
	pool_info.pPoolSizes = pool_sizes.data();

	VkDescriptorPool new_pool;
	vkCreateDescriptorPool(device, &pool_info, nullptr, &new_pool);
	return new_pool;
}

void descriptor_writer::write_image(const int binding, const VkImageView image, const VkSampler sampler, const VkImageLayout layout,
                                    const VkDescriptorType type)
{
	const VkDescriptorImageInfo& info = image_infos.emplace_back(VkDescriptorImageInfo{
		.sampler = sampler,
		.imageView = image,
		.imageLayout = layout
	});

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;

	writes.push_back(write);
}

void descriptor_writer::write_sampled_image(const int binding, const VkImageView image, const VkImageLayout layout,
	const VkDescriptorType type)
{
	const VkDescriptorImageInfo& info = image_infos.emplace_back(VkDescriptorImageInfo{
		.sampler = nullptr,
		.imageView = image,
		.imageLayout = layout
		});

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;

	writes.push_back(write);
}

void descriptor_writer::write_sampler(const int binding, const VkSampler sampler, const VkImageLayout layout,
	const VkDescriptorType type)
{
	const VkDescriptorImageInfo& info = image_infos.emplace_back(VkDescriptorImageInfo{
		.sampler = sampler,
		.imageView = nullptr,
		.imageLayout = layout
		});

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;

	writes.push_back(write);
}

void descriptor_writer::write_buffer(const int binding, const VkBuffer buffer, const size_t size, const size_t offset,
                                     const VkDescriptorType type)
{
	const VkDescriptorBufferInfo& info = buffer_infos.emplace_back(VkDescriptorBufferInfo{
		.buffer = buffer,
		.offset = offset,
		.range = size
	});

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE; // left empty for now until we need to write it
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = &info;

	writes.push_back(write);
}

void descriptor_writer::clear()
{
	image_infos.clear();
	writes.clear();
	buffer_infos.clear();
}

void descriptor_writer::update_set(const VkDevice device, const VkDescriptorSet set)
{
	for (VkWriteDescriptorSet& write : writes) {
		write.dstSet = set;
	}

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

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

descriptor_sets_manager::descriptor_sets_manager() :
	storage_images(descriptor_max_storage_image_descriptors, descriptor_storage_image_binding),
	sampled_images(descriptor_max_sampled_image_descriptors, descriptor_sampled_image_binding),
	samples(descriptor_max_sample_descriptors, descriptor_sample_binding)
{
}

VkResult descriptor_sets_manager::init(const VkDevice device)
{

	const auto pool_sizes = std::vector<VkDescriptorPoolSize>({
	  {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = descriptor_max_storage_image_descriptors},
	  {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = descriptor_max_sampled_image_descriptors},
	  {.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = descriptor_max_sample_descriptors},
		});
	VkDescriptorPoolCreateInfo pool_create_info{};
	pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	pool_create_info.maxSets = 1;
	pool_create_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
	pool_create_info.pPoolSizes = pool_sizes.data();

	VkDescriptorPool desc_pool;
	VkResult result = vkCreateDescriptorPool(device, &pool_create_info, nullptr, &desc_pool);
	if (result != VK_SUCCESS) return result;
	descriptor_pool_ = desc_pool;


	const auto bindings = std::vector<VkDescriptorSetLayoutBinding>({
	  {storage_images.binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descriptor_max_storage_image_descriptors, VK_SHADER_STAGE_ALL},
	  {sampled_images.binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, descriptor_max_sampled_image_descriptors, VK_SHADER_STAGE_ALL},
	  {samples.binding, VK_DESCRIPTOR_TYPE_SAMPLER, descriptor_max_sample_descriptors, VK_SHADER_STAGE_ALL},
	});

	const auto bindings_flags = std::vector<VkDescriptorBindingFlags>({
	  {VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT},
	  {VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT},
	  {VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT},
		});

	assert(bindings.size() == bindings_flags.size());
	assert(pool_sizes.size() == bindings_flags.size());

	VkDescriptorSetLayoutBindingFlagsCreateInfo layout_flags{};
	layout_flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	layout_flags.bindingCount = static_cast<uint32_t>(bindings_flags.size());
	layout_flags.pBindingFlags = bindings_flags.data();

	VkDescriptorSetLayoutCreateInfo layout_create_info{};
	layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_create_info.pNext = &layout_flags;
	layout_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	layout_create_info.bindingCount = static_cast<uint32_t>(bindings.size());
	layout_create_info.pBindings = bindings.data();

	VkDescriptorSetLayout set_layout{};
	result = vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &set_layout);
	if (result != VK_SUCCESS) return result;
	descriptor_set_layout_ = set_layout;

	VkDescriptorSetAllocateInfo set_create_info{};
	set_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		set_create_info.descriptorPool = desc_pool;
	set_create_info.descriptorSetCount = 1;
	set_create_info.pSetLayouts = &set_layout;

	VkDescriptorSet set;
	result = vkAllocateDescriptorSets(device, &set_create_info, &set);
	if (result != VK_SUCCESS) return result;
	descriptor_set_ = set;
	return VK_SUCCESS;
}

VkResult descriptor_sets_manager::reset(const VkDevice device)
{
	if (descriptor_set_.has_value())
	{
		vkResetDescriptorPool(device, descriptor_pool_.value(), 0);
	}

	VkDescriptorSetAllocateInfo set_create_info{};
	set_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		set_create_info.descriptorPool = descriptor_pool_.value();
	set_create_info.descriptorSetCount = 1;
	set_create_info.pSetLayouts = &descriptor_set_layout_.value();

	VkDescriptorSet set;
	if (const VkResult result = vkAllocateDescriptorSets(device, &set_create_info, &set); result != VK_SUCCESS) return result;
	descriptor_set_ = set;
	storage_images.allocator.reset();
	sampled_images.allocator.reset();
	samples.allocator.reset();
	return VK_SUCCESS;
}

void descriptor_sets_manager::deinit(const VkDevice device)
{
	if (descriptor_set_.has_value())
	{
		vkResetDescriptorPool(device, descriptor_pool_.value(), 0);
	}
	if (descriptor_set_layout_.has_value())
	{
		vkDestroyDescriptorSetLayout(device, descriptor_set_layout_.value(), nullptr);
	}
	if (descriptor_pool_.has_value())
	{
		vkDestroyDescriptorPool(device, descriptor_pool_.value(), nullptr);
	}
}

uint32_t descriptor_sets_manager::write_sampled_image(const VkDevice device, const VkImageView image, const VkImageLayout layout)
{
	uint32_t descriptor_index{ 0 };
	const auto res = sampled_images.allocator.allocate();
	if (res.has_value())
	{
		descriptor_index = res.value();
	}
	else
	{
		rosy_utils::debug_print_a("Unable to allocate sampled image descriptors max: %d\n", sampled_images.allocator.max_indexes);
		return 0;
	}

	VkDescriptorImageInfo info{};
	info.sampler = nullptr;
	info.imageView = image;
	info.imageLayout = layout;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = sampled_images.binding;
	write.dstArrayElement = descriptor_index;
	write.dstSet = descriptor_set_.value();
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.pImageInfo = &info;

	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	return descriptor_index;
}

uint32_t descriptor_sets_manager::write_sampler(const VkDevice device, const VkSampler sampler, const VkImageLayout layout)
{
	uint32_t descriptor_index{ 0 };
	const auto res = samples.allocator.allocate();
	if (res.has_value())
	{
		descriptor_index = res.value();
	}
	else
	{
		rosy_utils::debug_print_a("Unable to allocate sample descriptors max: %d\n", samples.allocator.max_indexes);
		return 0;
	}

	VkDescriptorImageInfo info{};
	info.sampler = sampler;
	info.imageView = nullptr;
	info.imageLayout = layout;


	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = samples.binding;
	write.dstArrayElement = descriptor_index;
	write.dstSet = descriptor_set_.value();
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	write.pImageInfo = &info;

	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	return descriptor_index;
}

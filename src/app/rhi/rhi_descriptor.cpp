#include "rhi.h"

descriptor_sets_manager::descriptor_sets_manager() :
	storage_images(descriptor_max_storage_image_descriptors, descriptor_storage_image_binding),
	sampled_images(descriptor_max_sampled_image_descriptors, descriptor_sampled_image_binding),
	samples(descriptor_max_sample_descriptors, descriptor_sample_binding)
{
}

VkResult descriptor_sets_manager::init_pool(const VkDevice device)
{
	const auto pool_sizes = std::vector<VkDescriptorPoolSize>({
	  {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = descriptor_max_storage_image_descriptors},
	  {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = descriptor_max_sampled_image_descriptors},
	  {.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = descriptor_max_sample_descriptors},
		});
	pool_sizes_ = pool_sizes;
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

	if (result = init_sets(device); result != VK_SUCCESS) return result;
	return VK_SUCCESS;
}

VkResult descriptor_sets_manager::init_sets(const VkDevice device)
{
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
	assert(pool_sizes_.size() == bindings_flags.size());

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
	VkResult result = vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &set_layout);
	if (result != VK_SUCCESS) return result;
	descriptor_set_layout = set_layout;

	VkDescriptorSetAllocateInfo set_create_info{};
	set_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		set_create_info.descriptorPool = descriptor_pool_.value();
	set_create_info.descriptorSetCount = 1;
	set_create_info.pSetLayouts = &set_layout;

	VkDescriptorSet set;
	result = vkAllocateDescriptorSets(device, &set_create_info, &set);
	if (result != VK_SUCCESS) return result;
	descriptor_set = set;
	return VK_SUCCESS;
}

VkResult descriptor_sets_manager::reset(const VkDevice device)
{
	if (descriptor_set.has_value())
	{
		vkResetDescriptorPool(device, descriptor_pool_.value(), 0);
	}
	if (descriptor_set_layout.has_value())
	{
		vkDestroyDescriptorSetLayout(device, descriptor_set_layout.value(), nullptr);
	}

	if (const VkResult result = init_sets(device); result != VK_SUCCESS) return result;

	storage_images.allocator.reset();
	sampled_images.allocator.reset();
	samples.allocator.reset();
	return VK_SUCCESS;
}

void descriptor_sets_manager::deinit(const VkDevice device)
{
	if (descriptor_set.has_value())
	{
		vkResetDescriptorPool(device, descriptor_pool_.value(), 0);
	}
	if (descriptor_set_layout.has_value())
	{
		vkDestroyDescriptorSetLayout(device, descriptor_set_layout.value(), nullptr);
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
	write.dstSet = descriptor_set.value();
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
	write.dstSet = descriptor_set.value();
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	write.pImageInfo = &info;

	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	return descriptor_index;
}

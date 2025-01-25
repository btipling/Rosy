#include "Graphics.h"
#include <format>
#include <vector>
#include <stack>
#include <algorithm>

#include "volk/volk.h"
#include "vma/vk_mem_alloc.h"
#include "vulkan/vk_enum_string_helper.h"
#include <ktxvulkan.h>
#include <SDL3/SDL_vulkan.h>
#pragma warning(disable: 4100 4459)
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>
#pragma warning(default: 4100 4459)
#include <filesystem>

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"

using namespace rosy;

namespace tracy {
	// ReSharper disable once CppInconsistentNaming
	class VkCtx;
}

namespace {

	////  Descriptor Set Allocator

	constexpr uint32_t descriptor_sampled_image_binding{ 0 };
	constexpr uint32_t descriptor_sample_binding{ 1 };
	constexpr uint32_t descriptor_storage_image_binding{ 2 };

	constexpr uint32_t descriptor_max_storage_image_descriptors{ 100'000 };
	constexpr uint32_t descriptor_max_sampled_image_descriptors{ 100'000 };
	constexpr uint32_t descriptor_max_sample_descriptors{ 1000 };

	constexpr double pi{ 3.1415926535897932384626433832795028841971693993751058209749445923078164062 };  // NOLINT(modernize-use-std-numbers)
	constexpr size_t debug_draw_circle_num_segments{ 100 };

	struct descriptor_set_allocator
	{
		uint32_t max_indices{ 1000 };
		std::stack<uint32_t> recycled_indices;
		uint32_t num_allocated{ 0 };

		result allocate(uint32_t* index)
		{
			uint32_t new_index = num_allocated;
			if (recycled_indices.empty())
			{
				if (num_allocated >= max_indices)
					return rosy::result::overflow;
				num_allocated += 1;
			}
			else
			{
				new_index = recycled_indices.top();
				recycled_indices.pop();
			}
			*index = new_index;
			return rosy::result::ok;
		}

		void free(const uint32_t index)
		{
			recycled_indices.push(index);
		}
		void reset()
		{
			while (!recycled_indices.empty()) recycled_indices.pop();
			num_allocated = 0;
		}
	};

	////  Descriptor Set Manager

	struct descriptor_set_manager
	{
		uint32_t binding{ 0 };
		descriptor_set_allocator allocator;

		result init(const uint32_t new_max_indices, const uint32_t new_binding)
		{
			binding = new_binding;
			allocator = descriptor_set_allocator{};
			allocator.max_indices = new_max_indices;
			return result::ok;
		}
	};

	/// Graphics Device

	constexpr  uint64_t graphics_created_bit_instance         = 0b0000000000000000000000000000000000000000000000000000000000000001;
	constexpr  uint64_t graphics_created_bit_device	          = 0b0000000000000000000000000000000000000000000000000000000000000010;
	constexpr  uint64_t graphics_created_bit_surface          = 0b0000000000000000000000000000000000000000000000000000000000000100;
	constexpr  uint64_t graphics_created_bit_vma              = 0b0000000000000000000000000000000000000000000000000000000000001000;
	constexpr  uint64_t graphics_created_bit_debug_messenger  = 0b0000000000000000000000000000000000000000000000000000000000010000;
	constexpr  uint64_t graphics_created_bit_fence            = 0b0000000000000000000000000000000000000000000000000000000000100000;
	constexpr  uint64_t graphics_created_bit_command_pool     = 0b0000000000000000000000000000000000000000000000000000000001000000;
	constexpr  uint64_t graphics_created_bit_draw_image       = 0b0000000000000000000000000000000000000000000000000000000010000000;
	constexpr  uint64_t graphics_created_bit_depth_image      = 0b0000000000000000000000000000000000000000000000000000000100000000;
	constexpr  uint64_t graphics_created_bit_ui_pool          = 0b0000000000000000000000000000000000000000000000000000001000000000;
	constexpr  uint64_t graphics_created_bit_swapchain        = 0b0000000000000000000000000000000000000000000000000000010000000000;
	constexpr  uint64_t graphics_created_bit_ktx_vdi_info     = 0b0000000000000000000000000000000000000000000000000000100000000000;
	constexpr  uint64_t graphics_created_bit_descriptor_set   = 0b0000000000000000000000000000000000000000000000000001000000000000;
	constexpr  uint64_t graphics_created_bit_descriptor_pool  = 0b0000000000000000000000000000000000000000000000000010000000000000;
	constexpr  uint64_t graphics_created_bit_draw_image_view  = 0b0000000000000000000000000000000000000000000000000100000000000000;
	constexpr  uint64_t graphics_created_bit_depth_image_view = 0b0000000000000000000000000000000000000000000000001000000000000000;
	constexpr  uint64_t graphics_created_bit_image_semaphore  = 0b0000000000000000000000000000000000000000000000010000000000000000;
	constexpr  uint64_t graphics_created_bit_pass_semaphore   = 0b0000000000000000000000000000000000000000000000100000000000000000;
	constexpr  uint64_t graphics_created_bit_imgui_sdl        = 0b0000000000000000000000000000000000000000000001000000000000000000;
	constexpr  uint64_t graphics_created_bit_imgui_vk         = 0b0000000000000000000000000000000000000000000010000000000000000000;
	constexpr  uint64_t graphics_created_bit_imgui_ctx        = 0b0000000000000000000000000000000000000000000100000000000000000000;
	constexpr  uint64_t graphics_created_bit_vertex_buffer    = 0b0000000000000000000000000000000000000000001000000000000000000000;
	constexpr  uint64_t graphics_created_bit_index_buffer     = 0b0000000000000000000000000000000000000000010000000000000000000000;
	constexpr  uint64_t graphics_created_bit_shaders          = 0b0000000000000000000000000000000000000000100000000000000000000000;
	constexpr  uint64_t graphics_created_bit_pipeline_layout  = 0b0000000000000000000000000000000000000001000000000000000000000000;
	constexpr  uint64_t graphics_created_bit_scene_buffer     = 0b0000000000000000000000000000000000000010000000000000000000000000;
	constexpr  uint64_t graphics_created_bit_materials_buffer = 0b0000000000000000000000000000000000000100000000000000000000000000;
	constexpr  uint64_t graphics_created_bit_graphics_buffer  = 0b0000000000000000000000000000000000001000000000000000000000000000;
	constexpr  uint64_t graphics_created_bit_ktx_image        = 0b0000000000000000000000000000000000010000000000000000000000000000;
	constexpr  uint64_t graphics_created_bit_ktx_texture      = 0b0000000000000000000000000000000000100000000000000000000000000000;
	constexpr  uint64_t graphics_created_bit_sampler          = 0b0000000000000000000000000000000001000000000000000000000000000000;
	constexpr  uint64_t graphics_created_bit_csm_image        = 0b0000000000000000000000000000000010000000000000000000000000000000;
	constexpr  uint64_t graphics_created_bit_csm_sampler      = 0b0000000000000000000000000000000100000000000000000000000000000000;
	constexpr  uint64_t graphics_created_bit_csm_view_sampler = 0b0000000000000000000000000000001000000000000000000000000000000000;
	constexpr  uint64_t graphics_created_bit_csm_view_near    = 0b0000000000000000000000000000010000000000000000000000000000000000;
	constexpr  uint64_t graphics_created_bit_csm_view_middle  = 0b0000000000000000000000000000100000000000000000000000000000000000;
	constexpr  uint64_t graphics_created_bit_csm_view_far     = 0b0000000000000000000000000001000000000000000000000000000000000000;
	constexpr  uint64_t graphics_created_bit_debug_buffer     = 0b0000000000000000000000000010000000000000000000000000000000000000;

	const char* default_instance_layers[] = {
		//"VK_LAYER_LUNARG_api_dump",
		"VK_LAYER_KHRONOS_validation",
		"VK_LAYER_KHRONOS_shader_object",
		"VK_LAYER_KHRONOS_synchronization2",
	};

	const char* default_instance_extensions[] = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	};

	const char* default_device_extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef TRACY_ENABLE
		VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
#endif
		VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
		VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME,
#ifndef RENDERDOC
		VK_KHR_MAINTENANCE_5_EXTENSION_NAME,
		VK_KHR_MAINTENANCE_6_EXTENSION_NAME,
		VK_KHR_MAINTENANCE_7_EXTENSION_NAME,
#endif
		//VK_KHR_MAINTENANCE_8_EXTENSION_NAME,
		//VK_KHR_MULTIVIEW_EXTENSION_NAME,
	};

	constexpr uint8_t max_frames_in_flight = 3;

	rosy::log const* debug_callback_logger = nullptr; // Exists only for the purpose of the callback, this is also not thread safe.
	VkBool32 VKAPI_CALL debug_callback(
		[[maybe_unused]] const VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
		[[maybe_unused]] const VkDebugUtilsMessageSeverityFlagsEXT message_type,
		const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
		[[maybe_unused]] void* p_user_data)
	{
		if (!debug_callback_logger) return VK_FALSE;
		if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
			debug_callback_logger->error(std::format("[{}] {}",
				message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ? "Validation" : "Performance",
				p_callback_data->pMessage));
		}
		else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
			debug_callback_logger->warn(std::format("[{}] {}",
				message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ? "Validation" : "Performance",
				p_callback_data->pMessage));
		}
		else {
			debug_callback_logger->debug(std::format("[{}] {}",
				message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ? "Validation" : "Performance",
				p_callback_data->pMessage));
		}
		return VK_FALSE;
	}

	struct swap_chain_support_details {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};

	struct frame_data
	{
		uint64_t frame_graphics_created_bitmask{ 0 };
		VkCommandBuffer command_buffer{ nullptr };
		VkSemaphore image_available_semaphore{ nullptr };
		VkSemaphore render_finished_semaphore{};
		VkFence in_flight_fence{ nullptr };
		VkCommandPool command_pool{ nullptr };
	};

	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats)
	{
		for (const auto& available_format : available_formats)
		{
			if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace ==
				VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return available_format;
			}
		}
		return available_formats[0];
	}

	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes)
	{
		for (const auto& available_present_mode : available_present_modes)
		{
			if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return available_present_mode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}


	VkFilter  filter_to_val(const uint16_t filter)
	{
		switch (filter)
		{
		case 0:
		case 1:
		case 2:
			return VK_FILTER_NEAREST;
		case 3:
		case 4:
		case 5:
		default:
			return VK_FILTER_LINEAR;
		}
	}

	VkSamplerAddressMode wrap_to_val(const uint16_t wrap)
	{
		switch (wrap)
		{
		case 0:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case 1:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case 2:
		default:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}
	}

	struct gpu_scene_data
	{
		std::array<float, 16> view = { 0 };
		std::array<float, 16> proj = { 0 };
		std::array<float, 16> view_projection = { 0 };
		std::array<float, 4> sunlight = { 0 };
		std::array<float, 4> camera_position = { 0 };
		std::array<float, 4> ambient_color = { 0 };
		std::array<float, 4> sunlight_color = { 0 };
	};

	struct allocated_image
	{
		VkImage image;
		VkImageView image_view;
		VmaAllocation allocation;
		VkExtent3D image_extent;
		VkFormat image_format;
	};

	struct allocated_ktx_image
	{
		uint64_t graphics_created_bitmask{ 0 };
		ktxTexture* texture;
		ktxVulkanTexture vk_texture;
	};

	struct allocated_csm
	{
		VkImage image;
		VkImageView image_view_near;
		VkImageView image_view_middle;
		VkImageView image_view_far;
		VmaAllocation allocation;
		VkExtent3D image_extent;
		VkFormat image_format;
		VkSampler viewer_sampler;
		VkSampler shadow_sampler;
		VkDescriptorSet imgui_ds_near;
		VkDescriptorSet imgui_ds_middle;
		VkDescriptorSet imgui_ds_far;
		uint32_t ds_index_sampler;
		uint32_t ds_index_near;
		uint32_t ds_index_middle;
		uint32_t ds_index_far;
	};

	struct allocated_buffer
	{
		VkBuffer buffer;
		VmaAllocation allocation;
		VmaAllocationInfo info;
	};

	struct gpu_material
	{
		std::array<float, 4> color;
		float metallic_factor{ 0.f };
		float roughness_factor{ 0.f };
		uint32_t color_sampled_image_index{ UINT32_MAX };
		uint32_t color_sampler_index{ UINT32_MAX };
	};

	struct gpu_material_buffer
	{
		allocated_buffer material_buffer;
		VkDeviceAddress material_buffer_address;
	};

	struct gpu_debug_draws_buffer
	{
		allocated_buffer debug_draws_buffer;
		VkDeviceAddress debug_draws_buffer_address;
	};


	struct gpu_mesh_buffers
	{
		uint64_t graphics_created_bitmask{ 0 };
		allocated_buffer index_buffer;
		allocated_buffer vertex_buffer;
		VkDeviceAddress vertex_buffer_address;
		uint32_t num_indices{ 0 };
	};

	struct gpu_scene_buffers
	{
		allocated_buffer scene_buffer;
		VkDeviceAddress scene_buffer_address;
		size_t buffer_size;
	};

	struct graphic_objects_buffers
	{
		allocated_buffer go_buffer;
		VkDeviceAddress go_buffer_address;
		size_t buffer_size;
	};

	struct gpu_draw_push_constants
	{
		VkDeviceAddress scene_buffer{ 0 };
		VkDeviceAddress vertex_buffer{ 0 };
		VkDeviceAddress go_buffer{ 0 };
		VkDeviceAddress material_buffer{ 0 };
	};

	struct graphic_object_data
	{
		std::array<float, 16> transform;
		//std::array<float, 16> normal_transform;
	};

	struct graphics_stats {
		int triangle_count{ 0 };
		int line_count{ 0 };
		int draw_call_count{ 0 };
		float draw_time{ 0.f };
		float shadow_draw_time{ 0.f };
	};

	struct graphics_device
	{
		rosy::log const* l{ nullptr };
		config cfg{};
		uint64_t graphics_created_bitmask{ 0 };
		bool enable_validation_layers{ true };
		bool render_ui{ true };

		graphics_stats stats{};

		uint32_t current_frame{ 0 };
		uint32_t swapchain_image_index{ 0 };
		VkExtent2D draw_extent = {};

		std::vector<const char*> instance_layer_properties;
		std::vector<const char*> device_layer_properties;
		std::vector<const char*> instance_extensions;
		std::vector<const char*> device_extensions;

		VkInstance instance{ nullptr };
		VkDevice device{ nullptr };
		VkPhysicalDevice physical_device{ nullptr };
		VmaAllocator allocator{ nullptr };

		VkPhysicalDeviceProperties physical_device_properties{};
		VkPhysicalDeviceFeatures supported_features{};
		VkPhysicalDeviceMemoryProperties physical_device_memory_properties{};
		std::vector<VkQueueFamilyProperties> queue_family_properties;
		uint32_t queue_count{ 0 };
		uint32_t queue_index{ 0 };
		std::vector<float> queue_priorities;

		tracy::VkCtx* tracy_ctx{ nullptr };

		VkQueue present_queue{};

		swap_chain_support_details swapchain_details{};
		VkSwapchainKHR swapchain{ nullptr };
		VkSurfaceFormatKHR swapchain_image_format{};
		VkPresentModeKHR swapchain_present_mode{};
		uint8_t swapchain_image_count{ 0 };
		std::vector<frame_data> frame_datas;
		std::vector<VkImage> swapchain_images;
		std::vector<VkImageView> swapchain_image_views;
		VkExtent2D swapchain_extent{};

		descriptor_set_manager* desc_storage_images{ nullptr };
		descriptor_set_manager* desc_sampled_images{ nullptr };
		descriptor_set_manager* desc_samples{ nullptr };
		VkDescriptorSetLayout descriptor_set_layout{ nullptr };
		VkDescriptorPool descriptor_pool{ nullptr };
		std::vector<VkDescriptorPoolSize> pool_sizes;
		VkDescriptorSet descriptor_set{};

		VkDebugUtilsMessengerEXT debug_messenger{ nullptr };
		VkSurfaceKHR surface{ nullptr };
		VkPhysicalDeviceFeatures required_features{};

		allocated_image draw_image;
		allocated_image depth_image;
		allocated_csm shadow_map_image;
		uint32_t default_sampler_index{ 0 };
		VkSampler default_sampler{ nullptr };
		float render_scale = 1.f;

		VkDescriptorPool ui_pool{ nullptr };

		VkFence immediate_fence{ nullptr };
		VkCommandBuffer immediate_command_buffer{ nullptr };
		VkCommandPool immediate_command_pool{ nullptr };

		SDL_Window* window{ nullptr };
		ktxVulkanDeviceInfo ktx_vdi_info{};

		// Level dependent data
		std::vector< VkSampler> samplers;
		std::vector<VkImageView> image_views;
		std::vector<allocated_ktx_image> ktx_textures;
		std::vector<gpu_mesh_buffers> gpu_meshes{};
		gpu_material_buffer material_buffer{};
		gpu_debug_draws_buffer debug_draws_buffer{};
		graphic_objects_buffers graphic_objects_buffer{};
		std::vector<surface_graphics_data> surface_graphics{};
		std::vector<VkShaderEXT> scene_shaders;
		VkPipelineLayout scene_layout;

		// Buffers
		gpu_scene_buffers scene_buffer{};

		gpu_scene_data scene_data{};
		write_level_state* wls{ nullptr };

		result init(const config new_cfg)
		{
			cfg = new_cfg;
			VkResult vk_res = volkInitialize();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to initialize volk! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = query_instance_layers();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to query instance layers! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = query_instance_extensions();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to query instance extensions! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_instance();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to create Vulkan instance! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = create_debug_callback();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to create Vulkan debug callback! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_surface();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to create surface! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_physical_device();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to create Vulkan physical device! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = query_device_layers();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to query device layers! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = query_device_extensions();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to query device extensions! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_device();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to create Vulkan device {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_tracy();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to init tracy! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_allocator();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to init vma! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_presentation_queue();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to get presentation queue! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_swapchain();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to init swap chain! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_swapchain_failure;
			}

			vk_res = init_draw_image();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to init draw image! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_descriptors();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to init draw descriptors! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_command_pool();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to init command pool! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_command_buffers();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to init command buffers! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_sync_objects();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to init sync objects! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_ui();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to init UI! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_csm_image();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to init csm! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_commands();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to init commands! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_data();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to init data! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_default_sampler();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to init default sampler! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}

			vk_res = init_ktx();
			if (vk_res != VK_SUCCESS)
			{
				l->error(std::format("Failed to init ktx! {} {}", static_cast<uint8_t>(vk_res), string_VkResult(vk_res)));
				return result::graphics_init_failure;
			}
			return result::ok;
		}

		void deinit()
		{
			// WAIT FOR DEVICE IDLE BEGIN **** 

			if (graphics_created_bitmask & graphics_created_bit_device)
			{
				if (const VkResult res = vkDeviceWaitIdle(device); res != VK_SUCCESS)
				{
					l->error(std::format("Failed to wait device to be idle: {}", static_cast<uint8_t>(res)));
				}
			}

			// WAIT FOR DEVICE IDLE END **** 

			// Deinit acquired resources in the opposite order in which they were created

			for (const VkSampler& sampler : samplers)
			{
				vkDestroySampler(device, sampler, nullptr);
			}

			for (const VkImageView& image_view : image_views)
			{
				vkDestroyImageView(device, image_view, nullptr);
			}

			for (auto& [gfx_created_bitmask, ktx_texture, ktx_vk_texture] : ktx_textures)
			{
				if (gfx_created_bitmask & graphics_created_bit_ktx_image)
				{
					ktxTexture_Destroy(ktx_texture);
				}
				if (gfx_created_bitmask & graphics_created_bit_ktx_texture)
				{
					ktxVulkanTexture_Destruct(&ktx_vk_texture, device, nullptr);
				}
			}

			if (graphics_created_bitmask & graphics_created_bit_ktx_vdi_info)
			{
				ktxVulkanDeviceInfo_Destruct(&ktx_vdi_info);
			}

			{
				if (graphics_created_bitmask & graphics_created_bit_csm_image) {
					vmaDestroyImage(allocator, shadow_map_image.image, shadow_map_image.allocation);
					ImGui_ImplVulkan_RemoveTexture(shadow_map_image.imgui_ds_near);
					ImGui_ImplVulkan_RemoveTexture(shadow_map_image.imgui_ds_middle);
					ImGui_ImplVulkan_RemoveTexture(shadow_map_image.imgui_ds_far);
				}
				if (graphics_created_bitmask & graphics_created_bit_csm_image) {
					vkDestroySampler(device, shadow_map_image.viewer_sampler, nullptr);
				}
				if (graphics_created_bitmask & graphics_created_bit_csm_sampler) {
				vkDestroySampler(device, shadow_map_image.shadow_sampler, nullptr);
				}
				if (graphics_created_bitmask & graphics_created_bit_csm_view_near) {
					vkDestroyImageView(device, shadow_map_image.image_view_near, nullptr);
				}
				if (graphics_created_bitmask & graphics_created_bit_csm_view_middle) {
				vkDestroyImageView(device, shadow_map_image.image_view_middle, nullptr);
				}
				if (graphics_created_bitmask & graphics_created_bit_csm_view_far) {
					vkDestroyImageView(device, shadow_map_image.image_view_far, nullptr);
				}
			}

			if (graphics_created_bitmask & graphics_created_bit_debug_buffer)
			{
				vmaDestroyBuffer(allocator, debug_draws_buffer.debug_draws_buffer.buffer, debug_draws_buffer.debug_draws_buffer.allocation);
			}

			if (graphics_created_bitmask & graphics_created_bit_scene_buffer)
			{
				vmaDestroyBuffer(allocator, scene_buffer.scene_buffer.buffer, scene_buffer.scene_buffer.allocation);
			}

			if (graphics_created_bitmask & graphics_created_bit_pipeline_layout)
			{
				vkDestroyPipelineLayout(device, scene_layout, nullptr);
			}

			if (graphics_created_bitmask & graphics_created_bit_shaders)
			{
				for (const VkShaderEXT shader : scene_shaders)
				{
					vkDestroyShaderEXT(device, shader, nullptr);
				}
			}

			for (const auto& gpu_mesh : gpu_meshes) {
				if (gpu_mesh.graphics_created_bitmask & graphics_created_bit_index_buffer)
				{
					vmaDestroyBuffer(allocator, gpu_mesh.index_buffer.buffer, gpu_mesh.index_buffer.allocation);
				}

				if (gpu_mesh.graphics_created_bitmask & graphics_created_bit_vertex_buffer)
				{
					vmaDestroyBuffer(allocator, gpu_mesh.vertex_buffer.buffer, gpu_mesh.vertex_buffer.allocation);
				}
			}

			if (graphics_created_bitmask & graphics_created_bit_graphics_buffer)
			{
				vmaDestroyBuffer(allocator, graphic_objects_buffer.go_buffer.buffer, graphic_objects_buffer.go_buffer.allocation);
			}

			if (graphics_created_bitmask & graphics_created_bit_materials_buffer)
			{
				vmaDestroyBuffer(allocator, material_buffer.material_buffer.buffer, material_buffer.material_buffer.allocation);
			}

			if (graphics_created_bitmask & graphics_created_bit_sampler)
			{
				vkDestroySampler(device, default_sampler, nullptr);
			}

			if (graphics_created_bitmask & graphics_created_bit_imgui_vk)
			{
				ImGui_ImplVulkan_Shutdown();
			}

			if (graphics_created_bitmask & graphics_created_bit_imgui_sdl)
			{
				ImGui_ImplSDL3_Shutdown();
			}

			if (graphics_created_bitmask & graphics_created_bit_imgui_ctx)
			{
				ImGui::DestroyContext();
			}

			if (graphics_created_bitmask & graphics_created_bit_ui_pool)
			{
				vkDestroyDescriptorPool(device, ui_pool, nullptr);
			}

			if (graphics_created_bitmask & graphics_created_bit_command_pool)
			{
				vkDestroyCommandPool(device, immediate_command_pool, nullptr);
			}

			if (graphics_created_bitmask & graphics_created_bit_fence)
			{
				vkDestroyFence(device, immediate_fence, nullptr);
			}

			for (const frame_data fd : frame_datas)
			{
				if (fd.frame_graphics_created_bitmask & graphics_created_bit_fence) vkDestroyFence(device, fd.in_flight_fence, nullptr);
				if (fd.frame_graphics_created_bitmask & graphics_created_bit_image_semaphore)  vkDestroySemaphore(device, fd.image_available_semaphore, nullptr);
				if (fd.frame_graphics_created_bitmask & graphics_created_bit_pass_semaphore) vkDestroySemaphore(device, fd.render_finished_semaphore, nullptr);
				if (fd.frame_graphics_created_bitmask & graphics_created_bit_command_pool) vkDestroyCommandPool(device, fd.command_pool, nullptr);
			}

			if (graphics_created_bitmask & graphics_created_bit_depth_image_view)
			{
				vkDestroyImageView(device, depth_image.image_view, nullptr);
			}

			if (graphics_created_bitmask & graphics_created_bit_depth_image)
			{
				vmaDestroyImage(allocator, depth_image.image, depth_image.allocation);
			}

			if (graphics_created_bitmask & graphics_created_bit_draw_image_view)
			{
				vkDestroyImageView(device, draw_image.image_view, nullptr);
			}

			if (graphics_created_bitmask & graphics_created_bit_draw_image)
			{
				vmaDestroyImage(allocator, draw_image.image, draw_image.allocation);
			}

			if (desc_storage_images != nullptr)
			{
				delete desc_storage_images;
				desc_storage_images = nullptr;
			}

			if (desc_sampled_images != nullptr)
			{
				delete desc_sampled_images;
				desc_sampled_images = nullptr;
			}

			if (desc_samples != nullptr)
			{
				delete desc_samples;
				desc_samples = nullptr;
			}

			destroy_swapchain();

			if (graphics_created_bitmask & graphics_created_bit_descriptor_set)
			{
				vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
			}

			if (graphics_created_bitmask & graphics_created_bit_descriptor_pool)
			{
				vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
			}

			if (graphics_created_bitmask & graphics_created_bit_vma)
			{
				vmaDestroyAllocator(allocator);
			}

			if (tracy_ctx != nullptr) {
				TracyVkDestroy(tracy_ctx);
				tracy_ctx = nullptr;
			}

			if (graphics_created_bitmask & graphics_created_bit_device)
			{
				if (const VkResult res = vkDeviceWaitIdle(device); res == VK_SUCCESS) vkDestroyDevice(device, nullptr);
			}

			if (graphics_created_bitmask & graphics_created_bit_surface)
			{
				SDL_Vulkan_DestroySurface(instance, surface, nullptr);
			}

			if (graphics_created_bitmask & graphics_created_bit_debug_messenger)
			{
				vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
			}

			if (graphics_created_bitmask & graphics_created_bit_instance)
			{
				vkDestroyInstance(instance, nullptr);
			}
		}

		VkResult query_instance_layers()
		{
			l->info("Querying instance layers");
			VkResult res;

			uint32_t p_property_count = 0;
			{
				if (res = vkEnumerateInstanceLayerProperties(&p_property_count, nullptr); res != VK_SUCCESS) return res;
				l->debug(std::format("Found %d instance layers {}", p_property_count));
				if (p_property_count == 0) return res;
			}

			std::vector<VkLayerProperties> layers;
			{
				layers.resize(p_property_count);
				if (res = vkEnumerateInstanceLayerProperties(&p_property_count, layers.data()); res != VK_SUCCESS) return res;
				if (!enable_validation_layers) return res;
			}

			for (VkLayerProperties lp : layers)
			{
				l->debug(std::format("Instance layer name: {} layer description: {}", lp.layerName, lp.description));
				for (const char* layer_name : default_instance_layers)
				{
					if (strcmp(layer_name, lp.layerName) == 0)
					{
						l->debug(std::format("Adding instance layer: {}", lp.layerName));
						instance_layer_properties.push_back(layer_name);
					}
				}
			}
			return res;
		}

		VkResult query_instance_extensions()
		{
			l->info("Querying instance extensions");
			VkResult res;

			uint32_t p_property_count = 0;
			{
				if (res = vkEnumerateInstanceExtensionProperties(nullptr, &p_property_count, nullptr); res != VK_SUCCESS) return res;
				l->debug(std::format("Found {} instance extensions", p_property_count));
				if (p_property_count == 0) return res;
			}

			std::vector<VkExtensionProperties> extensions;
			{
				extensions.resize(p_property_count);
				if (res = vkEnumerateInstanceExtensionProperties(nullptr, &p_property_count, extensions.data()); res != VK_SUCCESS) return res;
				l->debug(std::format("num required instance extensions: {}", std::size(default_instance_extensions)));
			}

			{
				// Setup required instance extensions
				uint32_t extension_count;
				const auto extension_names = SDL_Vulkan_GetInstanceExtensions(&extension_count);
				for (uint32_t i = 0; i < extension_count; i++)
				{
					l->debug(std::format("pushing back required SDL instance extension with name: {}", extension_names[i]));
					instance_extensions.push_back(extension_names[i]);
				}
				for (uint32_t i = 0; i < std::size(default_instance_extensions); i++)
				{
					l->debug(std::format("pushing back required rosy instance extension with name: {}", default_instance_extensions[i]));
					instance_extensions.push_back(default_instance_extensions[i]);
				}
			}
			l->debug(std::format("num instanceExtensions: {}", instance_extensions.size()));

			std::vector<const char*> required_instance_extensions(std::begin(instance_extensions), std::end(instance_extensions));

			for (auto& [extensionName, specVersion] : extensions)
			{
				l->debug(std::format("Instance extension name: {}", extensionName));
				for (const char* extension_name : instance_extensions)
				{
					if (strcmp(extension_name, extensionName) == 0)
					{
						l->debug(std::format("Requiring instance extension: {}", extension_name));
						std::erase(required_instance_extensions, extension_name);
					}
				}
			}
			if (required_instance_extensions.size() != 0) return VK_ERROR_EXTENSION_NOT_PRESENT;
			return res;
		}

		VkResult init_instance()
		{
			l->info("Initializing instance");

			VkApplicationInfo app_info{
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pApplicationName = "Rosy",
				.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
				.pEngineName = "Rosy",
				.engineVersion = VK_MAKE_VERSION(1, 0, 0),
				.apiVersion = VK_API_VERSION_1_3,
			};

			constexpr VkDebugUtilsMessengerCreateInfoEXT create_debug_callback_info_ext = {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				.pNext = nullptr,
				.flags = 0,
				.messageSeverity =
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
				.messageType =
					VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
				.pfnUserCallback = debug_callback,
				.pUserData = nullptr,
			};

			const VkInstanceCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pNext = &create_debug_callback_info_ext,
				.pApplicationInfo = &app_info,
				.enabledLayerCount = static_cast<uint32_t>(instance_layer_properties.size()),
				.ppEnabledLayerNames = instance_layer_properties.data(),
				.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size()),
				.ppEnabledExtensionNames = instance_extensions.data(),
			};


			if (const VkResult res = vkCreateInstance(&create_info, nullptr, &instance);  res != VK_SUCCESS) return res;
			l->debug("Vulkan instance created successfully!");
			volkLoadInstance(instance);

			// Set the debug callback logger to use the graphics device logger.
			debug_callback_logger = l;
			graphics_created_bitmask |= graphics_created_bit_instance;
			return VK_SUCCESS;
		}

		VkResult create_debug_callback()
		{
			l->info("Creating Vulkan debug callback");
			if (!enable_validation_layers) return VK_SUCCESS;

			constexpr VkDebugUtilsMessengerCreateInfoEXT create_debug_callback_info_ext = {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				.pNext = nullptr,
				.flags = 0,
				.messageSeverity =
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
				.messageType =
					VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
					VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
				.pfnUserCallback = debug_callback,
				.pUserData = nullptr,
			};
			if (const VkResult res = vkCreateDebugUtilsMessengerEXT(instance, &create_debug_callback_info_ext, nullptr, &debug_messenger); res != VK_SUCCESS) 
			{
				return res;
			}
			graphics_created_bitmask |= graphics_created_bit_debug_messenger;
			return VK_SUCCESS;
		}

		VkResult init_surface()
		{
			l->info("Initializing SDL Surface");
			if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface))
			{
				l->error(std::format("Failed to create SDL Vulkan surface: {}", SDL_GetError()));
				return VK_ERROR_INITIALIZATION_FAILED;
			}
			graphics_created_bitmask |= graphics_created_bit_surface;
			return VK_SUCCESS;
		}

		VkResult init_physical_device()
		{
			l->info("Initializing physical device");
			std::vector<VkPhysicalDevice> physical_devices;

			uint32_t physical_device_count = 0;
			if (VkResult res = vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr); res != VK_SUCCESS)
			{
				return res;
			}

			physical_devices.resize(physical_device_count);
			vkEnumeratePhysicalDevices(instance, &physical_device_count, &physical_devices[0]);

			// Shader features
			required_features.multiDrawIndirect  = VK_TRUE;
			required_features.tessellationShader = VK_TRUE;
			required_features.geometryShader     = VK_TRUE;
			required_features.shaderInt64        = VK_TRUE;

			// Rendering features
			required_features.fillModeNonSolid   = VK_TRUE;
			required_features.wideLines          = VK_TRUE;

			// Depth features
			required_features.depthBiasClamp     = VK_TRUE;
			required_features.depthClamp         = VK_TRUE;
			required_features.depthBounds        = VK_TRUE;

			for (const VkPhysicalDevice& p_device : physical_devices)
			{
				// get device properties
				VkPhysicalDeviceProperties device_properties;
				vkGetPhysicalDeviceProperties(p_device, &device_properties);

				bool swap_chain_adequate = false;
				swap_chain_support_details swap_chain_support = query_swap_chain_support(p_device);
				swap_chain_adequate = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();
				if (!swap_chain_adequate) continue;

				// shader objects required
				VkPhysicalDeviceShaderObjectFeaturesEXT shader_object_features{};
				shader_object_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
				shader_object_features.pNext = nullptr;

				VkPhysicalDeviceDepthClipEnableFeaturesEXT  depth_clip_features{};
				depth_clip_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT;
				depth_clip_features.pNext = &shader_object_features;

				VkPhysicalDeviceFeatures2 device_features2{};
				device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				device_features2.pNext = &depth_clip_features;
				vkGetPhysicalDeviceFeatures2(p_device, &device_features2);

				if (!shader_object_features.shaderObject) continue;

				// dynamic rendering required
				VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features{};
				buffer_device_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
				buffer_device_address_features.pNext = nullptr;

				device_features2 = {};
				device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				device_features2.pNext = &buffer_device_address_features;
				vkGetPhysicalDeviceFeatures2(p_device, &device_features2);

				if (!buffer_device_address_features.bufferDeviceAddress) continue;

				// data device address required
				VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features{};
				dynamic_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
				dynamic_rendering_features.pNext = nullptr;

				device_features2 = {};
				device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				device_features2.pNext = &dynamic_rendering_features;
				vkGetPhysicalDeviceFeatures2(p_device, &device_features2);

				if (!dynamic_rendering_features.dynamicRendering) continue;

				// features
				VkPhysicalDeviceFeatures features;
				vkGetPhysicalDeviceFeatures(p_device, &features);
				if (features.multiDrawIndirect != VK_TRUE) continue;
				if (features.tessellationShader != VK_TRUE) continue;
				if (features.geometryShader != VK_TRUE) continue;
				if (features.shaderInt64 != VK_TRUE) continue;

				if (features.fillModeNonSolid != VK_TRUE) continue;
				if (features.wideLines != VK_TRUE) continue;

				if (features.depthBiasClamp != VK_TRUE) continue;
				if (features.depthClamp != VK_TRUE) continue;
				if (features.depthBounds != VK_TRUE) continue;

				// memory
				VkPhysicalDeviceMemoryProperties mem_props;
				vkGetPhysicalDeviceMemoryProperties(p_device, &mem_props);

				// queues
				uint32_t new_queue_count = 0;
				uint32_t new_queue_index = 0;
				std::vector<VkQueueFamilyProperties> current_queue_family_properties_data;
				{
					vkGetPhysicalDeviceQueueFamilyProperties(p_device, &new_queue_count, nullptr);
					current_queue_family_properties_data.resize(new_queue_count);
					vkGetPhysicalDeviceQueueFamilyProperties(p_device, &new_queue_count, &current_queue_family_properties_data[0]);
				}

				for (std::uint32_t i = 0; i < current_queue_family_properties_data.size(); ++i)
				{
					VkQueueFamilyProperties q_props = current_queue_family_properties_data[i];
					if (q_props.timestampValidBits < 64) continue;

					if (!(q_props.queueFlags & (
						VK_QUEUE_GRAPHICS_BIT |
						VK_QUEUE_COMPUTE_BIT |
						VK_QUEUE_TRANSFER_BIT |
						VK_QUEUE_SPARSE_BINDING_BIT))) continue;

					VkBool32 present_support = false;
					vkGetPhysicalDeviceSurfaceSupportKHR(p_device, i, surface, &present_support);
					if (!present_support) continue;
					// Get the physical device with the largest queue count if all other requirements have been met.
					if (q_props.queueCount > queue_count)
					{
						new_queue_index = i;
						new_queue_count = q_props.queueCount;
						physical_device = p_device;
						physical_device_properties = device_properties;
						supported_features = features;
						physical_device_memory_properties = mem_props;
						queue_index = new_queue_index;
						queue_count = new_queue_count;
						queue_family_properties = current_queue_family_properties_data;
					}
				}
			}
			l->debug("Vulkan physical device created successfully!");
			return VK_SUCCESS;
		}

		[[nodiscard]] VkResult query_device_layers() const
		{
			l->info("Querying device layers");
			uint32_t p_property_count = 0;
			VkResult res = vkEnumerateDeviceLayerProperties(physical_device, &p_property_count, nullptr);
			if (res != VK_SUCCESS) return res;
			l->debug(std::format("Found {} device layers", p_property_count));
			if (p_property_count == 0) return res;
			std::vector<VkLayerProperties> layers;
			layers.resize(p_property_count);
			res = vkEnumerateDeviceLayerProperties(physical_device, &p_property_count, layers.data());
			if (res != VK_SUCCESS) return res;
			for (VkLayerProperties lp : layers)
			{
				l->debug(std::format("Device layer name: {} layer description: {}", lp.layerName, lp.description));
			}
			return res;
		}

		VkResult query_device_extensions()
		{
			l->info("Querying device extensions");

			uint32_t p_property_count = 0;

			VkResult res = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &p_property_count, nullptr);
			if (res != VK_SUCCESS) return res;

			l->debug(std::format("Found {} device extensions", p_property_count));
			if (p_property_count == 0) return res;

			std::vector<VkExtensionProperties> extensions;
			extensions.resize(p_property_count);

			res = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &p_property_count, extensions.data());
			if (res != VK_SUCCESS) return res;

			// validate required device extensions
			std::vector<const char*> required_device_extensions(std::begin(default_device_extensions), std::end(default_device_extensions));

			for (auto& [extensionName, specVersion] : extensions)
			{
				l->debug(std::format("Device extension name: {}", extensionName));
				for (const char* extension_name : default_device_extensions)
				{
					if (strcmp(extension_name, extensionName) == 0)
					{
						l->debug(std::format("Requiring device extension: {}", extension_name));
						device_extensions.push_back(extension_name);
						std::erase(required_device_extensions, extension_name);
					}
				}
			}

			if (required_device_extensions.size() != 0)
			{
				return VK_ERROR_EXTENSION_NOT_PRESENT;
			}
			return res;
		}

		VkResult init_device()
		{
			l->info("Initializing device");

			VkDeviceQueueCreateInfo device_queue_create_info{};
			device_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			device_queue_create_info.pNext = nullptr;
			device_queue_create_info.flags = 0;
			device_queue_create_info.queueFamilyIndex = queue_index;
			queue_priorities.resize(queue_count, 0.5f);
			device_queue_create_info.pQueuePriorities = queue_priorities.data();
			device_queue_create_info.queueCount = queue_count;

			VkPhysicalDeviceVulkan13Features vulkan13_features{};
			vulkan13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
			vulkan13_features.pNext = nullptr;
			vulkan13_features.dynamicRendering = VK_TRUE;
			vulkan13_features.synchronization2 = VK_TRUE;
			vulkan13_features.maintenance4 = VK_TRUE;

			VkPhysicalDeviceVulkan12Features vulkan12_features{};
			vulkan12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
			vulkan12_features.pNext = &vulkan13_features;
			vulkan12_features.bufferDeviceAddress = VK_TRUE;
			vulkan12_features.descriptorIndexing = VK_TRUE;
			vulkan12_features.shaderInputAttachmentArrayDynamicIndexing = VK_TRUE;
			vulkan12_features.shaderUniformTexelBufferArrayDynamicIndexing = VK_TRUE;
			vulkan12_features.shaderStorageTexelBufferArrayDynamicIndexing = VK_TRUE;
			vulkan12_features.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
			vulkan12_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
			vulkan12_features.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
			vulkan12_features.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
			vulkan12_features.shaderUniformTexelBufferArrayNonUniformIndexing = VK_TRUE;
			vulkan12_features.shaderStorageTexelBufferArrayNonUniformIndexing = VK_TRUE;
			vulkan12_features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
			vulkan12_features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
			vulkan12_features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
			vulkan12_features.descriptorBindingUniformTexelBufferUpdateAfterBind = VK_TRUE;
			vulkan12_features.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
			vulkan12_features.descriptorBindingPartiallyBound = VK_TRUE;
			vulkan12_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
			vulkan12_features.runtimeDescriptorArray = VK_TRUE;
			vulkan12_features.samplerFilterMinmax = VK_TRUE;
			vulkan12_features.scalarBlockLayout = VK_TRUE;
			vulkan12_features.imagelessFramebuffer = VK_TRUE;
			vulkan12_features.uniformBufferStandardLayout = VK_TRUE;
			vulkan12_features.shaderSubgroupExtendedTypes = VK_TRUE;
#ifdef TRACY_ENABLE
			vulkan12_features.hostQueryReset = VK_TRUE;
#endif

			VkPhysicalDeviceVulkan11Features vulkan11_features{};
			vulkan11_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
			vulkan11_features.pNext = &vulkan12_features;
			vulkan11_features.variablePointers = VK_TRUE;
			vulkan11_features.variablePointersStorageBuffer = VK_TRUE;
			vulkan11_features.multiview = VK_FALSE;
			vulkan11_features.multiviewGeometryShader = VK_FALSE;
			vulkan11_features.multiviewTessellationShader = VK_FALSE;

			VkPhysicalDeviceShaderObjectFeaturesEXT enable_shader_object = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,
				.pNext = &vulkan11_features,
				.shaderObject = VK_TRUE
			};

			VkPhysicalDeviceDepthClipEnableFeaturesEXT  enable_depth_clip_object = {
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT,
				.pNext = &enable_shader_object,
				.depthClipEnable = VK_TRUE
			};

			VkDeviceCreateInfo device_create_info = {};
			device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			device_create_info.pNext = &enable_depth_clip_object;
			device_create_info.flags = 0;
			device_create_info.queueCreateInfoCount = 1;
			device_create_info.pQueueCreateInfos = &device_queue_create_info;
			device_create_info.enabledLayerCount = 0;
			device_create_info.ppEnabledLayerNames = nullptr;
			device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
			device_create_info.ppEnabledExtensionNames = device_extensions.data();
			device_create_info.pEnabledFeatures = &required_features;

			
			if (VkResult res = vkCreateDevice(physical_device, &device_create_info, nullptr, &device); res != VK_SUCCESS)
			{
				return res;
			}

			l->debug("Vulkan device created successfully!");
			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_INSTANCE;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(instance);
				debug_name.pObjectName = "rosy instance";
				if (VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
			}
			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_PHYSICAL_DEVICE;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(physical_device);
				debug_name.pObjectName = "rosy physical device";
				if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
				{
					l->error(std::format("Error creating physical device name: {}", static_cast<uint8_t>(res)));
					return res;
				}
			}
			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_DEVICE;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(device);
				debug_name.pObjectName = "rosy device";
				if (VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
			}
			graphics_created_bitmask |= graphics_created_bit_device;
			return VK_SUCCESS;
		}

		VkResult init_tracy()
		{
			l->info("Initializing Tracy");
			tracy_ctx = TracyVkContextHostCalibrated(
				physical_device,
				device,
				vkResetQueryPool,
				vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
				vkGetCalibratedTimestampsEXT);
			return VK_SUCCESS;
		}

		VkResult init_allocator()
		{
			l->info("Initializing VMA");

			constexpr VkDeviceSize device_size{ 0 };
			VmaVulkanFunctions vulkan_functions{};
			vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
			vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

			VmaAllocatorCreateInfo allocator_create_info{};
			allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_3;
			allocator_create_info.physicalDevice = physical_device;
			allocator_create_info.device = device;
			allocator_create_info.instance = instance;
			allocator_create_info.pVulkanFunctions = &vulkan_functions;
			allocator_create_info.preferredLargeHeapBlockSize = device_size;
			allocator_create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

			vmaCreateAllocator(&allocator_create_info, &allocator);
			graphics_created_bitmask |= graphics_created_bit_vma;
			return VK_SUCCESS;
		}

		VkResult init_presentation_queue()
		{
			l->info("Initializing presentation queue");
			{
				VkDeviceQueueInfo2 get_info{};
				get_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
				get_info.flags = 0;
				get_info.queueFamilyIndex = queue_index;
				get_info.queueIndex = 0;
				vkGetDeviceQueue2(device, &get_info, &present_queue);
			}
			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_QUEUE;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(present_queue);
				debug_name.pObjectName = "rosy present queue";
				if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
			}
			return VK_SUCCESS;
		}

		[[nodiscard]] VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) const
		{
			if (constexpr uint32_t max_u32 = (std::numeric_limits<uint32_t>::max)(); capabilities.currentExtent.width !=
				max_u32)
			{
				return capabilities.currentExtent;
			}
			int width, height;
			SDL_GetWindowSizeInPixels(window, &width, &height);

			VkExtent2D actual_extent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width,
				capabilities.maxImageExtent.width);
			actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height,
				capabilities.maxImageExtent.height);

			return actual_extent;
		}

		void destroy_swapchain()
		{
			vkDeviceWaitIdle(device);
			for (const VkImageView image_view : swapchain_image_views)
			{
				vkDestroyImageView(device, image_view, nullptr);
			}
			swapchain_image_views.clear();
			if (graphics_created_bitmask & graphics_created_bit_swapchain)
			{
				vkDestroySwapchainKHR(device, swapchain, nullptr);
				graphics_created_bitmask &= ~graphics_created_bit_swapchain;
			}
		}

		VkResult create_swapchain()
		{
			{
				swapchain_details = query_swap_chain_support(physical_device);

				swapchain_image_format = choose_swap_surface_format(swapchain_details.formats);
				swapchain_present_mode = choose_swap_present_mode(swapchain_details.present_modes);

				swapchain_image_count = static_cast<uint8_t>(swapchain_details.capabilities.minImageCount);
				if (swapchain_details.capabilities.maxImageCount > 0 && swapchain_image_count < swapchain_details.capabilities.maxImageCount)
				{
					swapchain_image_count = static_cast<uint8_t>(swapchain_details.capabilities.maxImageCount);
				}
				swapchain_image_count = std::min(max_frames_in_flight, swapchain_image_count);
				frame_datas.resize(swapchain_image_count);
			}

			const VkExtent2D extent = choose_swap_extent(swapchain_details.capabilities);

			{
				VkSwapchainCreateInfoKHR swapchain_create_info{};
				swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
				swapchain_create_info.surface = surface;
				swapchain_create_info.minImageCount = swapchain_image_count;
				swapchain_create_info.imageFormat = swapchain_image_format.format;
				swapchain_create_info.imageColorSpace = swapchain_image_format.colorSpace;
				swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

				swapchain_create_info.imageExtent = extent;
				swapchain_create_info.imageArrayLayers = 1;
				swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

				// Just one queue family right now.
				swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
				swapchain_create_info.queueFamilyIndexCount = 0;
				swapchain_create_info.pQueueFamilyIndices = nullptr;

				swapchain_create_info.preTransform = swapchain_details.capabilities.currentTransform;

				swapchain_create_info.presentMode = swapchain_present_mode;
				swapchain_create_info.clipped = VK_TRUE;

				swapchain_create_info.oldSwapchain = nullptr;

				if (const VkResult res = vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain); res != VK_SUCCESS) return res;
				graphics_created_bitmask |= graphics_created_bit_swapchain;
				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(swapchain);
					debug_name.pObjectName = "rosy swapchain";
					if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
				}
			}

			{
				swapchain_extent = extent;

				swapchain_images.clear();
				auto count = static_cast<uint32_t>(swapchain_image_count);
				if (const auto res = vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr); res != VK_SUCCESS) return res;
				swapchain_images.resize(swapchain_image_count);
				if (const auto res = vkGetSwapchainImagesKHR(device, swapchain, &count, swapchain_images.data()); res != VK_SUCCESS) return res;

				for (size_t i = 0; i < swapchain_images.size(); i++)
				{
					VkImageViewCreateInfo swap_chain_image_view_create_info{};
					swap_chain_image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
					swap_chain_image_view_create_info.pNext = nullptr;
					swap_chain_image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
					swap_chain_image_view_create_info.image = swapchain_images[i];
					swap_chain_image_view_create_info.format = swapchain_image_format.format;
					swap_chain_image_view_create_info.subresourceRange.baseMipLevel = 0;
					swap_chain_image_view_create_info.subresourceRange.levelCount = 1;
					swap_chain_image_view_create_info.subresourceRange.baseArrayLayer = 0;
					swap_chain_image_view_create_info.subresourceRange.layerCount = 1;
					swap_chain_image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					VkImageView image_view{};
					if (const VkResult res = vkCreateImageView(device, &swap_chain_image_view_create_info, nullptr, &image_view); res != VK_SUCCESS) return res;
					// don't initially size these so we can clean this up nicely if any fail
					swapchain_image_views.push_back(image_view);
				}
			}
			return VK_SUCCESS;
		}

		result resize_swapchain()
		{
			vkDeviceWaitIdle(device);
			destroy_swapchain();
			if (const auto res = create_swapchain(); res != VK_SUCCESS)
			{
				l->error(std::format("Error recreating swapchain on resize: {}", static_cast<uint8_t>(res)));
				return result::graphics_swapchain_failure;
			}
			return result::ok;
		}

		VkResult init_swapchain()
		{
			l->info("Initializing swap chain");
			return create_swapchain();
		}

		VkResult init_draw_image()
		{
			l->info("Initializing draw image");

			VkResult res = VK_SUCCESS;
			const VkExtent3D draw_image_extent = {
				.width = static_cast<uint32_t>(cfg.max_window_width),
				.height = static_cast<uint32_t>(cfg.max_window_height),
				.depth = 1
			};

			VmaAllocationCreateInfo r_img_alloc_info{};
			r_img_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
			r_img_alloc_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			{
				// Draw image creation.
				draw_image.image_format = VK_FORMAT_R16G16B16A16_SFLOAT;
				draw_image.image_extent = draw_image_extent;

				VkImageUsageFlags draw_image_usages{};
				draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
				draw_image_usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				draw_image_usages |= VK_IMAGE_USAGE_STORAGE_BIT;
				draw_image_usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

				{
					VkImageCreateInfo draw_info{};
					draw_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
					draw_info.pNext = nullptr;
					draw_info.imageType = VK_IMAGE_TYPE_2D;
					draw_info.format = draw_image.image_format;
					draw_info.extent = draw_image_extent;
					draw_info.mipLevels = 1;
					draw_info.arrayLayers = 1;
					draw_info.samples = VK_SAMPLE_COUNT_1_BIT;
					draw_info.tiling = VK_IMAGE_TILING_OPTIMAL;
					draw_info.usage = draw_image_usages;

					if (res = vmaCreateImage(allocator, &draw_info, &r_img_alloc_info, &draw_image.image, &draw_image.allocation, nullptr); res != VK_SUCCESS) return res;
					graphics_created_bitmask |= graphics_created_bit_draw_image;
				}
				{
					VkImageViewCreateInfo draw_view_create_info{};
					draw_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
					draw_view_create_info.pNext = nullptr;
					draw_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
					draw_view_create_info.image = draw_image.image;
					draw_view_create_info.format = draw_image.image_format;
					draw_view_create_info.subresourceRange.baseMipLevel = 0;
					draw_view_create_info.subresourceRange.levelCount = 1;
					draw_view_create_info.subresourceRange.baseArrayLayer = 0;
					draw_view_create_info.subresourceRange.layerCount = 1;
					draw_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

					res = vkCreateImageView(device, &draw_view_create_info, nullptr, &draw_image.image_view);
					if (res != VK_SUCCESS) return res;
					graphics_created_bitmask |= graphics_created_bit_draw_image_view;
				}

				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_IMAGE;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(draw_image.image);
					debug_name.pObjectName = "rosy draw image";
					if (res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
				}
				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(draw_image.image_view);
					debug_name.pObjectName = "rosy draw image view";
					if (res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
				}
			}
			{
				// Depth image creation.
				depth_image.image_format = VK_FORMAT_D32_SFLOAT;
				depth_image.image_extent = draw_image_extent;
				VkImageUsageFlags depth_image_usages{};
				depth_image_usages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

				{
					VkImageCreateInfo depth_image_info{};
					depth_image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
					depth_image_info.pNext = nullptr;
					depth_image_info.imageType = VK_IMAGE_TYPE_2D;
					depth_image_info.format = depth_image.image_format;
					depth_image_info.extent = depth_image.image_extent;
					depth_image_info.mipLevels = 1;
					depth_image_info.arrayLayers = 1;
					depth_image_info.samples = VK_SAMPLE_COUNT_1_BIT;
					depth_image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
					depth_image_info.usage = depth_image_usages;

					if (res = vmaCreateImage(allocator, &depth_image_info, &r_img_alloc_info, &depth_image.image, &depth_image.allocation, nullptr); res != VK_SUCCESS) return res;
					graphics_created_bitmask |= graphics_created_bit_depth_image;
				}
				{
					VkImageViewCreateInfo depth_view_create_info{};
					depth_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
					depth_view_create_info.pNext = nullptr;
					depth_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
					depth_view_create_info.image = depth_image.image;
					depth_view_create_info.format = depth_image.image_format;
					depth_view_create_info.subresourceRange.baseMipLevel = 0;
					depth_view_create_info.subresourceRange.levelCount = 1;
					depth_view_create_info.subresourceRange.baseArrayLayer = 0;
					depth_view_create_info.subresourceRange.layerCount = 1;
					depth_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

					res = vkCreateImageView(device, &depth_view_create_info, nullptr, &depth_image.image_view);
					if (res != VK_SUCCESS) return res;
					graphics_created_bitmask |= graphics_created_bit_depth_image_view;
				}
				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_IMAGE;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(depth_image.image);
					debug_name.pObjectName = "rosy depth image";
					if (res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
				}
				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(depth_image.image_view);
					debug_name.pObjectName = "rosy depth image view";
					if (res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
				}
			}

			return res;
		}

		VkResult init_descriptors()
		{
			l->info("Initializing descriptors");

			// Descriptor set managers
			{
				desc_storage_images = new(std::nothrow) descriptor_set_manager;
				if (desc_storage_images == nullptr)
				{
					return VK_ERROR_OUT_OF_HOST_MEMORY;
				}
				desc_storage_images->init(descriptor_max_storage_image_descriptors, descriptor_storage_image_binding);
			}
			{
				desc_sampled_images = new(std::nothrow) descriptor_set_manager;
				if (desc_sampled_images == nullptr)
				{
					return VK_ERROR_OUT_OF_HOST_MEMORY;
				}
				desc_sampled_images->init(descriptor_max_sampled_image_descriptors, descriptor_sampled_image_binding);
			}
			{
				desc_samples = new(std::nothrow) descriptor_set_manager;
				if (desc_samples == nullptr)
				{
					return VK_ERROR_OUT_OF_HOST_MEMORY;
				}
				desc_samples->init(descriptor_max_sample_descriptors, descriptor_sample_binding);
			}

			pool_sizes = std::vector<VkDescriptorPoolSize>({
			  {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = descriptor_max_storage_image_descriptors},
			  {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = descriptor_max_sampled_image_descriptors},
			  {.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = descriptor_max_sample_descriptors},
				});

			{
				VkDescriptorPoolCreateInfo pool_create_info{};
				pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
				pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
				pool_create_info.maxSets = 1;
				pool_create_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
				pool_create_info.pPoolSizes = pool_sizes.data();
				if (const VkResult res = vkCreateDescriptorPool(device, &pool_create_info, nullptr, &descriptor_pool); res != VK_SUCCESS) return res;
				graphics_created_bitmask |= graphics_created_bit_descriptor_pool;
			}

			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_DESCRIPTOR_POOL;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(descriptor_pool);
				debug_name.pObjectName = "rosy descriptor pool";
				if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
				{
					l->error(std::format("Error creating descriptor pool name: {}", static_cast<uint8_t>(res)));
					return res;
				}
			}

			const auto bindings = std::vector<VkDescriptorSetLayoutBinding>({
				{desc_storage_images->binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descriptor_max_storage_image_descriptors, VK_SHADER_STAGE_ALL},
				{desc_sampled_images->binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, descriptor_max_sampled_image_descriptors, VK_SHADER_STAGE_ALL},
				{desc_samples->binding, VK_DESCRIPTOR_TYPE_SAMPLER, descriptor_max_sample_descriptors, VK_SHADER_STAGE_ALL},
				});

			const auto bindings_flags = std::vector<VkDescriptorBindingFlags>({
				{VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT},
				{VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT},
				{VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT},
				});

			if (bindings.size() != bindings_flags.size()) return VK_ERROR_INITIALIZATION_FAILED;
			if (pool_sizes.size() != bindings_flags.size()) return VK_ERROR_INITIALIZATION_FAILED;

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

			
			if (const VkResult res = vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &descriptor_set_layout); res != VK_SUCCESS)
			{
				return res;
			}

			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(descriptor_set_layout);
				debug_name.pObjectName = "rosy descriptor set layout";
				if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
				{
					l->error(std::format("Error creating descriptor set layout name: {}", static_cast<uint8_t>(res)));
					return res;
				}
			}

			VkDescriptorSetAllocateInfo set_create_info{};
			set_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			set_create_info.descriptorPool = descriptor_pool;
			set_create_info.descriptorSetCount = 1;
			set_create_info.pSetLayouts = &descriptor_set_layout;

			if (const VkResult res = vkAllocateDescriptorSets(device, &set_create_info, &descriptor_set); res != VK_SUCCESS) return res;
			graphics_created_bitmask |= graphics_created_bit_descriptor_set;

			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(descriptor_set);
				debug_name.pObjectName = "rosy descriptor set";
				if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
				{
					l->error(std::format("Error creating descriptor set name: {}", static_cast<uint8_t>(res)));
					return res;
				}
			}

			return VK_SUCCESS;
		}

		VkResult init_command_pool()
		{
			l->info("Initializing command pool");
			VkCommandPoolCreateInfo pool_info{};
			pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			pool_info.queueFamilyIndex = queue_index;

			for (size_t i = 0; i < swapchain_image_count; i++)
			{
				VkCommandPool command_pool{};
				if (const VkResult res = vkCreateCommandPool(device, &pool_info, nullptr, &command_pool); res != VK_SUCCESS) {
					return res;
				}
				frame_datas[i].command_pool = command_pool;
				frame_datas[i].frame_graphics_created_bitmask |= graphics_created_bit_command_pool;
				{
					const auto obj_name = std::format("rosy command pool {}", i);
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_COMMAND_POOL;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(command_pool);
					debug_name.pObjectName = obj_name.c_str();
					if (const auto res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
				}
			}
			return VK_SUCCESS;
		}

		[[nodiscard]] VkResult init_command_buffers()
		{
			l->info("Initializing command buffer");

			for (size_t i = 0; i < swapchain_image_count; i++)
			{
				{
					// render command buffer
					VkCommandBufferAllocateInfo alloc_info{};
					alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
					alloc_info.commandPool = frame_datas[i].command_pool;
					alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
					alloc_info.commandBufferCount = 1;

					VkCommandBuffer command_buffer{};
					if (const VkResult res = vkAllocateCommandBuffers(device, &alloc_info, &command_buffer); res != VK_SUCCESS) return res;
					frame_datas[i].command_buffer = command_buffer;
					{
						const auto obj_name = std::format("rosy command buffer {}", i);
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(command_buffer);
						debug_name.pObjectName = obj_name.c_str();
						if (const auto res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
					}
				}
			}
			return VK_SUCCESS;
		}

		VkResult init_sync_objects()
		{
			l->info("Initializing sync objects");

			VkSemaphoreCreateInfo semaphore_info{};
			semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VkFenceCreateInfo fence_info{};
			fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			VkResult res;
			for (size_t i = 0; i < swapchain_image_count; i++)
			{
				{
					VkSemaphore semaphore;
					res = vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphore);
					if (res != VK_SUCCESS) return res;
					frame_datas[i].image_available_semaphore = semaphore;
					frame_datas[i].frame_graphics_created_bitmask |= graphics_created_bit_image_semaphore;
					{
						const auto obj_name = std::format("rosy image available semaphore {}", i);
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_SEMAPHORE;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(semaphore);
						debug_name.pObjectName = obj_name.c_str();
						if (res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
					}
				}
				{
					VkSemaphore semaphore;
					res = vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphore);
					if (res != VK_SUCCESS) return res;
					frame_datas[i].render_finished_semaphore = semaphore;
					frame_datas[i].frame_graphics_created_bitmask |= graphics_created_bit_pass_semaphore;
					{
						const auto obj_name = std::format("rosy render finished semaphore {}", i);
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_SEMAPHORE;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(semaphore);
						debug_name.pObjectName = obj_name.c_str();
						if (res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
					}
				}
				{
					VkFence fence;
					res = vkCreateFence(device, &fence_info, nullptr, &fence);
					if (res != VK_SUCCESS) return res;
					frame_datas[i].in_flight_fence = fence;
					frame_datas[i].frame_graphics_created_bitmask |= graphics_created_bit_fence;
					{
						const auto obj_name = std::format("rosy in flight fence {}", i);
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_FENCE;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(fence);
						debug_name.pObjectName = obj_name.c_str();
						if (res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
					}
				}
			}
			{
				res = vkCreateFence(device, &fence_info, nullptr, &immediate_fence);
				if (res != VK_SUCCESS) return res;
				graphics_created_bitmask |= graphics_created_bit_fence;
				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_FENCE;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(immediate_fence);
					debug_name.pObjectName = "rosy immediate fence";
					if (res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
				}
			}
			return VK_SUCCESS;
		}

		VkResult init_ui()
		{
			l->info("Initializing UI");

			const VkDescriptorPoolSize imgui_pool_sizes[] = {
				{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
			};

			VkDescriptorPoolCreateInfo pool_info{};
			pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
			pool_info.maxSets = 1000;
			pool_info.poolSizeCount = static_cast<uint32_t>(std::size(imgui_pool_sizes));
			pool_info.pPoolSizes = imgui_pool_sizes;

			if (const VkResult res = vkCreateDescriptorPool(device, &pool_info, nullptr, &ui_pool); res != VK_SUCCESS) return res;
			graphics_created_bitmask |= graphics_created_bit_ui_pool;

			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_DESCRIPTOR_POOL;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(ui_pool);
				debug_name.pObjectName = "rosy ui descriptor pool";
				if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
				{
					l->error(std::format("Error creating ui descriptor pool name: {}", static_cast<uint8_t>(res)));
					return res;
				}
			}

			if (const auto ctx = ImGui::CreateContext(); ctx == nullptr)
			{
				l->error("Imgui create context failed");
				return VK_ERROR_INITIALIZATION_FAILED;
			}
			graphics_created_bitmask |= graphics_created_bit_imgui_ctx;

			if (!ImGui_ImplSDL3_InitForVulkan(window))
			{
				l->error("Imgui Vulkan SDL3 init failed");
				return VK_ERROR_INITIALIZATION_FAILED;
			}
			graphics_created_bitmask |= graphics_created_bit_imgui_sdl;

			ImGui_ImplVulkan_InitInfo init_info{};
			init_info.Instance = instance;
			init_info.PhysicalDevice = physical_device;
			init_info.Device = device;
			init_info.Queue = present_queue;
			init_info.DescriptorPool = ui_pool;
			init_info.MinImageCount = swapchain_details.capabilities.minImageCount;
			init_info.ImageCount = swapchain_image_count;
			init_info.UseDynamicRendering = true;

			init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
			init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
			init_info.PipelineRenderingCreateInfo.depthAttachmentFormat = depth_image.image_format;
			init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &draw_image.image_format;

			init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

			if (!ImGui_ImplVulkan_Init(&init_info))
			{
				l->error("Failed to init imgui");
				return VK_ERROR_INITIALIZATION_FAILED;
			}
			graphics_created_bitmask |= graphics_created_bit_imgui_vk;

			if (!ImGui_ImplVulkan_CreateFontsTexture())
			{
				l->error("Failed to init imgui font texture");
				return VK_ERROR_INITIALIZATION_FAILED;
			}

			int displays_count = 0;
			const auto display_ids = SDL_GetDisplays(&displays_count);
			if (display_ids == nullptr || displays_count <= 0)
			{
				l->error("Failed to get SDL display info");
				return VK_ERROR_INITIALIZATION_FAILED;
			}
			const float content_scale = SDL_GetDisplayContentScale(*display_ids);

			ImGuiIO& io = ImGui::GetIO();
			io.FontGlobalScale = content_scale;

			return VK_SUCCESS;
		}

		VkResult init_csm_image()
		{
			l->info("Initializing cascading shadow map");

			VkExtent3D draw_image_extent = {
				.width = static_cast<uint32_t>(cfg.max_window_width),
				.height = static_cast<uint32_t>(cfg.max_window_height),
				.depth = 1
			};

			VmaAllocationCreateInfo r_img_alloc_info{};
			r_img_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
			r_img_alloc_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			{
				// Shadow map image creation.
				constexpr VkExtent3D shadow_map_image_extent = {
					.width = 8192,
					.height = 8192,
					.depth = 1
				};
				shadow_map_image.image_format = VK_FORMAT_D32_SFLOAT;
				shadow_map_image.image_extent = shadow_map_image_extent;

				{
					VkImageUsageFlags depth_image_usages{ 0 };
					depth_image_usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
						VK_IMAGE_USAGE_TRANSFER_DST_BIT | 
						VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | 
						VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | 
						VK_IMAGE_USAGE_SAMPLED_BIT;

					VkImageCreateInfo depth_info{};
					depth_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
					depth_info.pNext = nullptr;
					depth_info.imageType = VK_IMAGE_TYPE_2D;
					depth_info.format = shadow_map_image.image_format;
					depth_info.extent = shadow_map_image_extent;
					depth_info.mipLevels = 1;
					depth_info.arrayLayers = 3;
					depth_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
					depth_info.samples = VK_SAMPLE_COUNT_1_BIT;
					depth_info.tiling = VK_IMAGE_TILING_OPTIMAL;
					depth_info.usage = depth_image_usages;

					if (VkResult res = vmaCreateImage(allocator, &depth_info, &r_img_alloc_info, &shadow_map_image.image, &shadow_map_image.allocation, nullptr); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating shadow map image: {}", static_cast<uint8_t>(res)));
						return res;
					}
					{
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_IMAGE;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(shadow_map_image.image);
						debug_name.pObjectName = "rosy shadow map image";
						if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
						{
							l->error(std::format("Error creating rosy shadow map name: {}", static_cast<uint8_t>(res)));
							return res;
						}
					}
					graphics_created_bitmask |= graphics_created_bit_csm_image;
				}

				{
					VkImageViewCreateInfo d_view_info{};
					d_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
					d_view_info.pNext = nullptr;
					d_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
					d_view_info.image = shadow_map_image.image;
					d_view_info.format = shadow_map_image.image_format;
					d_view_info.subresourceRange.baseMipLevel = 0;
					d_view_info.subresourceRange.levelCount = 1;
					d_view_info.subresourceRange.baseArrayLayer = 0;
					d_view_info.subresourceRange.layerCount = 1;
					d_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
					{

						if (VkResult res = vkCreateImageView(device, &d_view_info, nullptr, &shadow_map_image.image_view_near);  res != VK_SUCCESS) return res;
						graphics_created_bitmask |= graphics_created_bit_csm_view_near;
						{
							VkDebugUtilsObjectNameInfoEXT debug_name{};
							debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
							debug_name.pNext = nullptr;
							debug_name.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
							debug_name.objectHandle = reinterpret_cast<uint64_t>(shadow_map_image.image_view_near);
							debug_name.pObjectName = "rosy shadow map image view near";
							if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
							{
								l->error(std::format("Error creating shadow image view near name: {}", static_cast<uint8_t>(res)));
								return res;
							}
						}
						d_view_info.subresourceRange.baseArrayLayer = 1;

						if (VkResult res = vkCreateImageView(device, &d_view_info, nullptr, &shadow_map_image.image_view_middle); res != VK_SUCCESS) return res;
						graphics_created_bitmask |= graphics_created_bit_csm_view_middle;
						{
							VkDebugUtilsObjectNameInfoEXT debug_name{};
							debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
							debug_name.pNext = nullptr;
							debug_name.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
							debug_name.objectHandle = reinterpret_cast<uint64_t>(shadow_map_image.image_view_middle);
							debug_name.pObjectName = "rosy shadow map image view middle";
							if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
							{
								l->error(std::format("Error creating shadow image view middle name: {}", static_cast<uint8_t>(res)));
								return res;
							}
						}
						d_view_info.subresourceRange.baseArrayLayer = 2;

						if (VkResult res = vkCreateImageView(device, &d_view_info, nullptr, &shadow_map_image.image_view_far); res != VK_SUCCESS) return res;
						graphics_created_bitmask |= graphics_created_bit_csm_view_far;
						{
							VkDebugUtilsObjectNameInfoEXT debug_name{};
							debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
							debug_name.pNext = nullptr;
							debug_name.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
							debug_name.objectHandle = reinterpret_cast<uint64_t>(shadow_map_image.image_view_far);
							debug_name.pObjectName = "rosy shadow map image view far";
							if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
							{
								l->error(std::format("Error creating shadow image view far name: {}", static_cast<uint8_t>(res)));
								return res;
							}
						}
					}
					{
						VkSamplerCreateInfo sampler_create_info{};
						sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
						sampler_create_info.pNext = nullptr;

						sampler_create_info.maxLod = VK_LOD_CLAMP_NONE;
						sampler_create_info.minLod = 0;

						sampler_create_info.mipLodBias = 0.f;
						sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
						sampler_create_info.anisotropyEnable = VK_FALSE;
						sampler_create_info.maxAnisotropy = 0.f;

						sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
						sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
						sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
						sampler_create_info.compareOp = VK_COMPARE_OP_NEVER;
						sampler_create_info.compareEnable = VK_FALSE;

						sampler_create_info.magFilter = VK_FILTER_NEAREST;
						sampler_create_info.minFilter = VK_FILTER_NEAREST;

						sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
						sampler_create_info.unnormalizedCoordinates = VK_FALSE;
						sampler_create_info.flags = 0;

						if (VkResult res = vkCreateSampler(device, &sampler_create_info, nullptr, &shadow_map_image.viewer_sampler); res != VK_SUCCESS) return res;
						graphics_created_bitmask |= graphics_created_bit_csm_view_sampler;
						{
							VkDebugUtilsObjectNameInfoEXT debug_name{};
							debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
							debug_name.pNext = nullptr;
							debug_name.objectType = VK_OBJECT_TYPE_SAMPLER;
							debug_name.objectHandle = reinterpret_cast<uint64_t>(shadow_map_image.viewer_sampler);
							debug_name.pObjectName = "rosy csm view sampler";
							if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
							{
								l->error(std::format("Error creating csm view sampler name: {}", static_cast<uint8_t>(res)));
								return res;
							}
						}
					}
				}
				{
					VkSamplerCreateInfo sampler_create_info{};
					sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
					sampler_create_info.pNext = nullptr;

					sampler_create_info.maxLod = VK_LOD_CLAMP_NONE;
					sampler_create_info.minLod = 0;

					sampler_create_info.mipLodBias = 0.f;
					sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
					sampler_create_info.anisotropyEnable = VK_FALSE;
					sampler_create_info.maxAnisotropy = 0.f;

					sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
					sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
					sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
					sampler_create_info.compareOp = VK_COMPARE_OP_NEVER;
					sampler_create_info.compareEnable = VK_FALSE;

					sampler_create_info.magFilter = VK_FILTER_LINEAR;
					sampler_create_info.minFilter = VK_FILTER_LINEAR;

					sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
					sampler_create_info.unnormalizedCoordinates = VK_FALSE;
					sampler_create_info.flags = 0;
					if (VkResult res = vkCreateSampler(device, &sampler_create_info, nullptr, &shadow_map_image.shadow_sampler); res != VK_SUCCESS) return res;
					graphics_created_bitmask |= graphics_created_bit_csm_sampler;

					{
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_SAMPLER;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(shadow_map_image.shadow_sampler);
						debug_name.pObjectName = "rosy csm shadow sampler";
						if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
						{
							l->error(std::format("Error creating csm shadow sampler name: {}", static_cast<uint8_t>(res)));
							return res;
						}
					}
				}
				{
					shadow_map_image.imgui_ds_near = ImGui_ImplVulkan_AddTexture(shadow_map_image.viewer_sampler, shadow_map_image.image_view_near, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
					shadow_map_image.imgui_ds_middle = ImGui_ImplVulkan_AddTexture(shadow_map_image.viewer_sampler, shadow_map_image.image_view_middle, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
					shadow_map_image.imgui_ds_far = ImGui_ImplVulkan_AddTexture(shadow_map_image.viewer_sampler, shadow_map_image.image_view_far, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
				}
				{
					{
						if (const result res = desc_samples->allocator.allocate(&default_sampler_index); res != result::ok)
						{
							l->error(std::format("Error allocating a shadow map sampler desc: {}", static_cast<uint8_t>(res)));
							return VK_ERROR_UNKNOWN;
						}
						{
							uint32_t new_sampler_desc_index{ 0 };
							if (const result res = desc_samples->allocator.allocate(&new_sampler_desc_index); res != result::ok)
							{
								l->error(std::format("Error default shadow map sampler index: {}", static_cast<uint8_t>(res)));
								return VK_ERROR_INITIALIZATION_FAILED;
							}
							{
								VkDescriptorImageInfo create_desc_info{};
								create_desc_info.sampler = shadow_map_image.shadow_sampler;
								create_desc_info.imageView = nullptr;
								create_desc_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;


								VkWriteDescriptorSet write{};
								write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
								write.dstBinding = desc_samples->binding;
								write.dstArrayElement = new_sampler_desc_index;
								write.dstSet = descriptor_set;
								write.descriptorCount = 1;
								write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
								write.pImageInfo = &create_desc_info;

								vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
							}
						}
					}
					{
						uint32_t new_image_sampler_desc_index{ 0 };
						if (const result res = desc_sampled_images->allocator.allocate(&new_image_sampler_desc_index); res != result::ok)
						{
							l->error(std::format("Error default shadow near map index: {}", static_cast<uint8_t>(res)));
							return VK_ERROR_INITIALIZATION_FAILED;
						}
						{
							VkDescriptorImageInfo create_desc_info{};
							create_desc_info.sampler = nullptr;
							create_desc_info.imageView = shadow_map_image.image_view_near;
							create_desc_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;


							VkWriteDescriptorSet write{};
							write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
							write.dstBinding = desc_sampled_images->binding;
							write.dstArrayElement = new_image_sampler_desc_index;
							write.dstSet = descriptor_set;
							write.descriptorCount = 1;
							write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
							write.pImageInfo = &create_desc_info;

							vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
						}
					}
					{
						uint32_t new_image_sampler_desc_index{ 0 };
						if (const result res = desc_sampled_images->allocator.allocate(&new_image_sampler_desc_index); res != result::ok)
						{
							l->error(std::format("Error default shadow middle map index: {}", static_cast<uint8_t>(res)));
							return VK_ERROR_INITIALIZATION_FAILED;
						}
						{
							VkDescriptorImageInfo create_desc_info{};
							create_desc_info.sampler = nullptr;
							create_desc_info.imageView = shadow_map_image.image_view_middle;
							create_desc_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

							VkWriteDescriptorSet write{};
							write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
							write.dstBinding = desc_sampled_images->binding;
							write.dstArrayElement = new_image_sampler_desc_index;
							write.dstSet = descriptor_set;
							write.descriptorCount = 1;
							write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
							write.pImageInfo = &create_desc_info;

							vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
						}
					}
					{
						uint32_t new_image_sampler_desc_index{ 0 };
						if (const result res = desc_sampled_images->allocator.allocate(&new_image_sampler_desc_index); res != result::ok)
						{
							l->error(std::format("Error default shadow far map index: {}", static_cast<uint8_t>(res)));
							return VK_ERROR_INITIALIZATION_FAILED;
						}
						{
							VkDescriptorImageInfo create_desc_info{};
							create_desc_info.sampler = nullptr;
							create_desc_info.imageView = shadow_map_image.image_view_far;
							create_desc_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;


							VkWriteDescriptorSet write{};
							write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
							write.dstBinding = desc_sampled_images->binding;
							write.dstArrayElement = new_image_sampler_desc_index;
							write.dstSet = descriptor_set;
							write.descriptorCount = 1;
							write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
							write.pImageInfo = &create_desc_info;

							vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
						}
					}
				}
			}

			return VK_SUCCESS;
		}

		VkResult init_commands()
		{
			l->info("Initializing commands");

			VkCommandPoolCreateInfo pool_info{};
			pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			pool_info.queueFamilyIndex = queue_index;

			if (const VkResult res = vkCreateCommandPool(device, &pool_info, nullptr, &immediate_command_pool); res != VK_SUCCESS)
			{
				return res;
			}

			graphics_created_bitmask |= graphics_created_bit_command_pool;
			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_COMMAND_POOL;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(immediate_command_pool);
				debug_name.pObjectName = "rosy immediate command pool";
				if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
			}

			// allocate the command data for immediate submits
			VkCommandBufferAllocateInfo alloc_info{};
			alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			alloc_info.commandPool = immediate_command_pool;
			alloc_info.commandBufferCount = 1;
			alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;


			if (const VkResult res = vkAllocateCommandBuffers(device, &alloc_info, &immediate_command_buffer); res != VK_SUCCESS)
			{
				return res;
			}
			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(immediate_command_buffer);
				debug_name.pObjectName = "rosy immediate command buffer";
				if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS) return res;
			}

			return VK_SUCCESS;
		}

		VkResult init_data()
		{
			l->info("Initializing data");
			{
				// *** SETTING SCENE BUFFER *** //
				scene_buffer.buffer_size = sizeof(gpu_scene_data);
				VkBufferCreateInfo buffer_info{};
				buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				buffer_info.pNext = nullptr;
				buffer_info.size = scene_buffer.buffer_size;
				buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

				VmaAllocationCreateInfo vma_alloc_info{};
				vma_alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
				vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

				if (
					const VkResult res = vmaCreateBuffer(
						allocator, &buffer_info, &vma_alloc_info, &scene_buffer.scene_buffer.buffer, &scene_buffer.scene_buffer.allocation,
						&scene_buffer.scene_buffer.info
					); res != VK_SUCCESS)
				{
					l->error(std::format("Error uploading scene buffer: {}", static_cast<uint8_t>(res)));
					return res;
				}
				graphics_created_bitmask |= graphics_created_bit_scene_buffer;
				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_BUFFER;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(scene_buffer.scene_buffer.buffer);
					debug_name.pObjectName = "rosy scene buffer";
					if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating scene buffer name: {}", static_cast<uint8_t>(res)));
						return res;
					}
				}
				{
					VkBufferDeviceAddressInfo device_address_info{};
					device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
					device_address_info.buffer = scene_buffer.scene_buffer.buffer;

					// *** SETTING SCENE BUFFER ADDRESS *** //
					scene_buffer.scene_buffer_address = vkGetBufferDeviceAddress(device, &device_address_info);
				}
			}
			{
				// *** SETTING DEBUG DRAWS BUFFER *** //
				std::vector<std::array<float, 4>> debug_vertices{};
				{
					// Gen debug vertices
					// A line
					debug_vertices.push_back({ 0.f, 0.f, 0.f, 1.f });
					debug_vertices.push_back({ 1.f, 0.f, 0.f, 1.f });
					// A cross
					debug_vertices.push_back({ 0.f, 0.f, 0.f, 1.f });
					debug_vertices.push_back({ 1.f, 0.f, 0.f, 1.f });
					debug_vertices.push_back({ 0.f, 0.f, 0.f, 1.f });
					debug_vertices.push_back({ 0.f, 1.f, 0.f, 1.f });
					debug_vertices.push_back({ 0.f, 0.f, 0.f, 1.f });
					debug_vertices.push_back({ 0.f, 1.f, 0.f, 1.f });
					// A circle
					constexpr float num_segments_f{ static_cast<float>(debug_draw_circle_num_segments) };
					constexpr float target{ static_cast<float>(pi) * 2 };
					constexpr float segment_step{ target / num_segments_f };
					for (size_t i{0}; i < debug_draw_circle_num_segments + 1; i++)
					{
						const float current_step{ segment_step * static_cast<float>(i + 1) };
						const float next_step = i == debug_draw_circle_num_segments + 1 ? 0.f : current_step + segment_step;
						debug_vertices.push_back({ std::sin(current_step), std::cos(current_step), 0.f, 1.f});
						debug_vertices.push_back({ std::sin(next_step), std::cos(next_step), 0.f, 1.f });
					}
				}

				const size_t debug_draws_buffer_size = sizeof(std::array<float, 4>) * debug_vertices.size();
				{
					VkBufferCreateInfo buffer_info{};
					buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
					buffer_info.pNext = nullptr;
					buffer_info.size = debug_draws_buffer_size;
					buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

					VmaAllocationCreateInfo vma_alloc_info{};
					vma_alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
					vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

					if (const VkResult res = vmaCreateBuffer(allocator, &buffer_info, &vma_alloc_info, &debug_draws_buffer.debug_draws_buffer.buffer, &debug_draws_buffer.debug_draws_buffer.allocation, &debug_draws_buffer.debug_draws_buffer.info); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating debug draws buffer: {}", static_cast<uint8_t>(res)));
						return res;
					}
					{
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_BUFFER;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(debug_draws_buffer.debug_draws_buffer.buffer);
						debug_name.pObjectName = "rosy debug draws buffer";
						if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
						{
							l->error(std::format("Error creating debug draws buffer name: {}", static_cast<uint8_t>(res)));
							return res;
						}
					}
				}

				{
					VkBufferDeviceAddressInfo device_address_info{};
					device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
					device_address_info.buffer = debug_draws_buffer.debug_draws_buffer.buffer;

					// *** SETTING DEBUG DRAWS BUFFER ADDRESS *** //
					debug_draws_buffer.debug_draws_buffer_address = vkGetBufferDeviceAddress(device, &device_address_info);
				}

				allocated_buffer staging{};
				{
					VkBufferCreateInfo buffer_info{};
					buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
					buffer_info.pNext = nullptr;
					buffer_info.size = debug_draws_buffer_size;
					buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

					VmaAllocationCreateInfo vma_alloc_info{};
					vma_alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
					vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

					if (const VkResult res = vmaCreateBuffer(allocator, &buffer_info, &vma_alloc_info, &staging.buffer, &staging.allocation, &staging.info); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating debug draws staging buffer: {}", static_cast<uint8_t>(res)));
						return res;
					}
					graphics_created_bitmask |= graphics_created_bit_debug_buffer;
					{
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_BUFFER;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(staging.buffer);
						debug_name.pObjectName = "rosy debug draws staging buffer";
						if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
						{
							l->error(std::format("Error creating debug draws staging buffer name: {}", static_cast<uint8_t>(res)));
							return res;
						}
					}
				}

				memcpy(staging.info.pMappedData, debug_vertices.data(), debug_draws_buffer_size);

				{
					if (VkResult res = vkResetFences(device, 1, &immediate_fence); res != VK_SUCCESS)
					{
						l->error(std::format("Error resetting immediate fence for debug draws buffer: {}", static_cast<uint8_t>(res)));
						return res;
					}

					if (VkResult res =  vkResetCommandBuffer(immediate_command_buffer, 0); res != VK_SUCCESS)
					{
						l->error(std::format("Error resetting immediate command buffer for debug draws buffer: {}", static_cast<uint8_t>(res)));
						return res;
					}

					VkCommandBufferBeginInfo begin_info{};
					begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
					begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

					if (VkResult res = vkBeginCommandBuffer(immediate_command_buffer, &begin_info); res != VK_SUCCESS)
					{
						l->error(std::format("Error beginning immediate command buffer for debug draws buffer: {}", static_cast<uint8_t>(res)));
						return res;
					}

					VkBufferCopy vertex_copy{ 0 };
					vertex_copy.dstOffset = 0;
					vertex_copy.srcOffset = 0;
					vertex_copy.size = debug_draws_buffer_size;

					vkCmdCopyBuffer(immediate_command_buffer, staging.buffer, debug_draws_buffer.debug_draws_buffer.buffer, 1, &vertex_copy);

					if (VkResult res = vkEndCommandBuffer(immediate_command_buffer); res != VK_SUCCESS)
					{
						l->error(std::format("Error ending immediate command buffer for debug draws buffer: {}", static_cast<uint8_t>(res)));
						return res;
					}

					VkCommandBufferSubmitInfo cmd_buffer_submit_info{};
					cmd_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
					cmd_buffer_submit_info.pNext = nullptr;
					cmd_buffer_submit_info.commandBuffer = immediate_command_buffer;
					cmd_buffer_submit_info.deviceMask = 0;
					VkSubmitInfo2 submit_info{};
					submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
					submit_info.pNext = nullptr;

					submit_info.waitSemaphoreInfoCount = 0;
					submit_info.pWaitSemaphoreInfos = nullptr;
					submit_info.signalSemaphoreInfoCount = 0;
					submit_info.pSignalSemaphoreInfos = nullptr;
					submit_info.commandBufferInfoCount = 1;
					submit_info.pCommandBufferInfos = &cmd_buffer_submit_info;

					if (VkResult res = vkQueueSubmit2(present_queue, 1, &submit_info, immediate_fence); res != VK_SUCCESS)
					{
						l->error(std::format("Error submitting staging buffer for debug draws buffer: {}", static_cast<uint8_t>(res)));
						return res;
					}

					if (VkResult res = vkWaitForFences(device, 1, &immediate_fence, true, 9999999999); res != VK_SUCCESS)
					{
						l->error(std::format("Error waiting for immediate fence for debug draws buffer: {}", static_cast<uint8_t>(res)));
						return res;
					}
				}
				vmaDestroyBuffer(allocator, staging.buffer, staging.allocation);
			}

			return VK_SUCCESS;
		}

		VkResult init_default_sampler()
		{
			VkSamplerCreateInfo sampler_create_info = {};
			sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler_create_info.pNext = nullptr;
			sampler_create_info.maxLod = VK_LOD_CLAMP_NONE;
			sampler_create_info.minLod = 0;
			sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			sampler_create_info.magFilter = VK_FILTER_NEAREST;
			sampler_create_info.minFilter = VK_FILTER_NEAREST;
			if (const VkResult res = vkCreateSampler(device, &sampler_create_info, nullptr, &default_sampler); res != VK_SUCCESS)
			{
				l->error(std::format("Error creating default sampler: {}", static_cast<uint8_t>(res)));
				return res;
			}
			graphics_created_bitmask |= graphics_created_bit_sampler;
			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_SAMPLER;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(default_sampler);
				debug_name.pObjectName = "rosy default sampler";
				if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
				{
					l->error(std::format("Error creating default sampler name: {}", static_cast<uint8_t>(res)));
					return res;
				}
			}
			{
				if (const result res = desc_samples->allocator.allocate(&default_sampler_index); res != result::ok)
				{
					l->error(std::format("Error allocating a default sampler desc: {}", static_cast<uint8_t>(res)));
					return VK_ERROR_UNKNOWN;
				}
				{
					uint32_t new_sampler_desc_index{ 0 };
					if (const result res = desc_samples->allocator.allocate(&new_sampler_desc_index); res != result::ok)
					{
						l->error(std::format("Error default sampler descriptor index: {}", static_cast<uint8_t>(res)));
						return VK_ERROR_INITIALIZATION_FAILED;
					}
					{
						VkDescriptorImageInfo create_desc_info{};
						create_desc_info.sampler = default_sampler;
						create_desc_info.imageView = nullptr;
						create_desc_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;


						VkWriteDescriptorSet write{};
						write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
						write.dstBinding = desc_samples->binding;
						write.dstArrayElement = new_sampler_desc_index;
						write.dstSet = descriptor_set;
						write.descriptorCount = 1;
						write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
						write.pImageInfo = &create_desc_info;

						vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
					}
				}
			}
			return VK_SUCCESS;
		}

		VkResult init_ktx()
		{
			l->info("Initializing ktx");
			ktxVulkanDeviceInfo_Construct(&ktx_vdi_info, physical_device, device, present_queue, immediate_command_pool, nullptr);
			graphics_created_bitmask |= graphics_created_bit_ktx_vdi_info;
			return VK_SUCCESS;
		}

		[[nodiscard]] swap_chain_support_details query_swap_chain_support(const VkPhysicalDevice p_device) const
		{
			swap_chain_support_details details = {};
			if (const VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(p_device, surface, &details.capabilities); res != VK_SUCCESS)
			{
				l->warn(std::format("Failed to get device surface capabilities: {}", static_cast<uint8_t>(res)));
				return details;
			}

			uint32_t format_count;
			vkGetPhysicalDeviceSurfaceFormatsKHR(p_device, surface, &format_count, nullptr);

			if (format_count != 0) {
				details.formats.resize(format_count);
				if (const VkResult res = vkGetPhysicalDeviceSurfaceFormatsKHR(p_device, surface, &format_count, details.formats.data()); res != VK_SUCCESS)
				{
					l->warn(std::format("Failed to get device surface formats: {}", static_cast<uint8_t>(res)));
					return details;
				}
			}

			uint32_t present_mode_count{ 0 };
			if (const VkResult res = vkGetPhysicalDeviceSurfacePresentModesKHR(p_device, surface, &present_mode_count, nullptr); res != VK_SUCCESS)
			{
				l->warn(std::format("Failed to get device surface formats: {}", static_cast<uint8_t>(res)));
				return details;
			}

			if (present_mode_count != 0) {
				details.present_modes.resize(present_mode_count);
				if (const VkResult res = vkGetPhysicalDeviceSurfacePresentModesKHR(p_device, surface, &present_mode_count, details.present_modes.data()); res != VK_SUCCESS)
				{
					l->warn(std::format("Failed to get device surface present modes: {}", static_cast<uint8_t>(res)));
					return details;
				}
			}

			return details;
		}

		result set_asset(const rosy_packager::asset& a)
		{

			// *** SETTING IMAGES *** //
			std::vector<uint32_t> color_image_sampler_desc_index;
			for (const auto& [asset_img_name] : a.images)
			{
				const char* ktx_path{};
				allocated_ktx_image new_ktx_img{};

				std::string img_name{ asset_img_name.begin(), asset_img_name.end() };
				std::filesystem::path img_path{ a.asset_path };
				img_path.replace_filename(std::format("{}.ktx2", img_name));
				l->debug(std::format("source: {} path: {} name: {}", a.asset_path, img_path.string(), img_name));
				std::wstring image_path_wide{ img_path.c_str() };
				std::string img_path_staging{ img_path.string() };
				ktx_path = img_path_staging.c_str();

				{
					if (ktx_error_code_e ktx_res = ktxTexture_CreateFromNamedFile(ktx_path, KTX_TEXTURE_CREATE_NO_FLAGS, &new_ktx_img.texture); ktx_res != KTX_SUCCESS) {
						l->error(std::format("ktx create texture failure: {}", static_cast<uint8_t>(ktx_res)));
						return result::create_failed;
					}
					new_ktx_img.graphics_created_bitmask |= graphics_created_bit_ktx_image;
				}

				{
					if (ktx_error_code_e ktx_res = ktxTexture_VkUploadEx(new_ktx_img.texture, &ktx_vdi_info, &new_ktx_img.vk_texture,
						VK_IMAGE_TILING_OPTIMAL,
						VK_IMAGE_USAGE_SAMPLED_BIT,
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); ktx_res != KTX_SUCCESS)
					{
						ktx_textures.push_back(new_ktx_img);
						l->error(std::format("ktx create vulkan texture failure: {}", static_cast<uint8_t>(ktx_res)));
						return result::create_failed;
					}
					new_ktx_img.graphics_created_bitmask |= graphics_created_bit_ktx_texture;
				}

				{
					VkImageViewCreateInfo image_view_info{};
					image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
					image_view_info.pNext = nullptr;
					image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
					image_view_info.image = new_ktx_img.vk_texture.image;
					image_view_info.format = new_ktx_img.vk_texture.imageFormat;
					image_view_info.subresourceRange.baseMipLevel = 0;
					image_view_info.subresourceRange.levelCount = new_ktx_img.vk_texture.levelCount;
					image_view_info.subresourceRange.baseArrayLayer = 0;
					image_view_info.subresourceRange.layerCount = new_ktx_img.vk_texture.layerCount;
					image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					VkImageView image_view{};
					if (VkResult res = vkCreateImageView(device, &image_view_info, nullptr, &image_view); res != VK_SUCCESS) {
						l->error(std::format("asset image view creation failure: {}", static_cast<uint8_t>(res)));
						return result::create_failed;
					}
					const size_t index = image_views.size();
					image_views.push_back(image_view);
					{
						const auto obj_name = std::format("rosy asset image view {}", index);
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(image_view);
						debug_name.pObjectName = obj_name.c_str();
						if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
						{
							l->error(std::format("Error creating asset image view name: {}", static_cast<uint8_t>(res)));
							return result::error;
						}
					}
					{
						uint32_t new_image_sampler_desc_index{ 0 };
						if (const result res = desc_sampled_images->allocator.allocate(&new_image_sampler_desc_index); res != result::ok)
						{
							l->error(std::format("Error allocating sampler descriptor index: {}", static_cast<uint8_t>(res)));
							return result::create_failed;
						}
						{
							VkDescriptorImageInfo info{};
							info.sampler = nullptr;
							info.imageView = image_view;
							info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;


							VkWriteDescriptorSet write{};
							write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
							write.dstBinding = desc_sampled_images->binding;
							write.dstArrayElement = new_image_sampler_desc_index;
							write.dstSet = descriptor_set;
							write.descriptorCount = 1;
							write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
							write.pImageInfo = &info;

							vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
						}
						color_image_sampler_desc_index.push_back(new_image_sampler_desc_index);
					}
					assert(image_views.size() == color_image_sampler_desc_index.size());
				}

				ktx_textures.push_back(new_ktx_img);
			}

			// *** SETTING SAMPLERS *** //
			std::vector<uint32_t> sampler_desc_index;
			{
				size_t sampler_index{ 0 };
				for (const rosy_packager::sampler new_sampler : a.samplers)
				{
					VkSamplerCreateInfo sampler_create_info = {};
					sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
					sampler_create_info.pNext = nullptr;
					sampler_create_info.maxLod = VK_LOD_CLAMP_NONE;
					sampler_create_info.minLod = 0;
					sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

					sampler_create_info.addressModeU = wrap_to_val(new_sampler.wrap_s);
					sampler_create_info.addressModeV = wrap_to_val(new_sampler.wrap_t);
					sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

					sampler_create_info.magFilter = filter_to_val(new_sampler.mag_filter);
					sampler_create_info.minFilter = filter_to_val(new_sampler.min_filter);
					VkSampler created_sampler{};
					if (const VkResult res = vkCreateSampler(device, &sampler_create_info, nullptr, &created_sampler); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating asset sampler: {}", static_cast<uint8_t>(res)));
						return result::create_failed;
					}
					samplers.push_back(created_sampler);
					{
						const auto obj_name = std::format("rosy asset sampler {}", sampler_index);
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_SAMPLER;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(created_sampler);
						debug_name.pObjectName = obj_name.c_str();
						if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
						{
							l->error(std::format("Error creating asset sampler name: {}", static_cast<uint8_t>(res)));
							return result::error;
						}
					}
					{
						uint32_t new_sampler_desc_index{ 0 };
						if (const result res = desc_samples->allocator.allocate(&new_sampler_desc_index); res != result::ok)
						{
							l->error(std::format("Error allocating sampler descriptor index: {}", static_cast<uint8_t>(res)));
							return result::create_failed;
						}
						{
							VkDescriptorImageInfo info{};
							info.sampler = created_sampler;
							info.imageView = nullptr;
							info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;


							VkWriteDescriptorSet write{};
							write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
							write.dstBinding = desc_samples->binding;
							write.dstArrayElement = new_sampler_desc_index;
							write.dstSet = descriptor_set;
							write.descriptorCount = 1;
							write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
							write.pImageInfo = &info;

							vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
						}
						sampler_desc_index.push_back(new_sampler_desc_index);
					}
					sampler_index += 1;
				}
				assert(samplers.size() == sampler_desc_index.size());
			}

			// *** SETTING MATERIAL BUFFER *** //
			{
				std::vector<gpu_material> materials{};
				materials.reserve(a.materials.size());
				for (rosy_packager::material m : a.materials)
				{
					uint32_t color_image_sampler_index = UINT32_MAX;
					uint32_t color_sampler_index = default_sampler_index;
					if (m.color_image_index < color_image_sampler_desc_index.size()) {
						color_image_sampler_index = color_image_sampler_desc_index[m.color_image_index];

						assert(ktx_textures.size() > m.color_image_index);

						if (m.color_sampler_index < sampler_desc_index.size())
						{
							color_sampler_index = sampler_desc_index[m.color_sampler_index];
						}
					}

					materials.push_back({
						.color = m.base_color_factor,
						.metallic_factor = m.metallic_factor,
						.roughness_factor = m.roughness_factor,
						.color_sampled_image_index = color_image_sampler_index,
						.color_sampler_index = color_sampler_index,
						});
				}

				const size_t material_buffer_size = materials.size() * sizeof(materials);
				{
					VkBufferCreateInfo buffer_info{};
					buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
					buffer_info.pNext = nullptr;
					buffer_info.size = material_buffer_size;
					buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

					VmaAllocationCreateInfo vma_alloc_info{};
					vma_alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
					vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

					if (const VkResult res = vmaCreateBuffer(allocator, &buffer_info, &vma_alloc_info, &material_buffer.material_buffer.buffer, &material_buffer.material_buffer.allocation, &material_buffer.material_buffer.info); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating materials buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}
					{
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_BUFFER;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(material_buffer.material_buffer.buffer);
						debug_name.pObjectName = "rosy material buffer";
						if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
						{
							l->error(std::format("Error creating material buffer name: {}", static_cast<uint8_t>(res)));
							return result::error;
						}
					}
				}

				{
					VkBufferDeviceAddressInfo device_address_info{};
					device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
					device_address_info.buffer = material_buffer.material_buffer.buffer;

					// *** SETTING MATERIAL BUFFER ADDRESS *** //
					material_buffer.material_buffer_address = vkGetBufferDeviceAddress(device, &device_address_info);
				}

				allocated_buffer staging{};
				{
					VkBufferCreateInfo buffer_info{};
					buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
					buffer_info.pNext = nullptr;
					buffer_info.size = material_buffer_size;
					buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

					VmaAllocationCreateInfo vma_alloc_info{};
					vma_alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
					vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

					if (const VkResult res = vmaCreateBuffer(allocator, &buffer_info, &vma_alloc_info, &staging.buffer, &staging.allocation, &staging.info); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating materials staging buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}
					graphics_created_bitmask |= graphics_created_bit_materials_buffer;
					{
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_BUFFER;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(staging.buffer);
						debug_name.pObjectName = "rosy material staging buffer";
						if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
						{
							l->error(std::format("Error creating material staging buffer name: {}", static_cast<uint8_t>(res)));
							return result::error;
						}
					}
				}

				memcpy(staging.info.pMappedData, materials.data(), material_buffer_size);

				{
					if (VkResult res = vkResetFences(device, 1, &immediate_fence); res != VK_SUCCESS)
					{
						l->error(std::format("Error resetting immediate fence for materials buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}

					if (VkResult res =  vkResetCommandBuffer(immediate_command_buffer, 0); res != VK_SUCCESS)
					{
						l->error(std::format("Error resetting immediate command buffer for materials buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}

					VkCommandBufferBeginInfo begin_info{};
					begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
					begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

					if (VkResult res = vkBeginCommandBuffer(immediate_command_buffer, &begin_info); res != VK_SUCCESS)
					{
						l->error(std::format("Error beginning immediate command buffer for materials buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}

					VkBufferCopy vertex_copy{ 0 };
					vertex_copy.dstOffset = 0;
					vertex_copy.srcOffset = 0;
					vertex_copy.size = material_buffer_size;

					vkCmdCopyBuffer(immediate_command_buffer, staging.buffer, material_buffer.material_buffer.buffer, 1, &vertex_copy);

					if (VkResult res = vkEndCommandBuffer(immediate_command_buffer); res != VK_SUCCESS)
					{
						l->error(std::format("Error ending immediate command buffer for materials buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}

					VkCommandBufferSubmitInfo cmd_buffer_submit_info{};
					cmd_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
					cmd_buffer_submit_info.pNext = nullptr;
					cmd_buffer_submit_info.commandBuffer = immediate_command_buffer;
					cmd_buffer_submit_info.deviceMask = 0;
					VkSubmitInfo2 submit_info{};
					submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
					submit_info.pNext = nullptr;

					submit_info.waitSemaphoreInfoCount = 0;
					submit_info.pWaitSemaphoreInfos = nullptr;
					submit_info.signalSemaphoreInfoCount = 0;
					submit_info.pSignalSemaphoreInfos = nullptr;
					submit_info.commandBufferInfoCount = 1;
					submit_info.pCommandBufferInfos = &cmd_buffer_submit_info;

					if (VkResult res = vkQueueSubmit2(present_queue, 1, &submit_info, immediate_fence); res != VK_SUCCESS)
					{
						l->error(std::format("Error submitting staging buffer for materials buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}

					if (VkResult res = vkWaitForFences(device, 1, &immediate_fence, true, 9999999999); res != VK_SUCCESS)
					{
						l->error(std::format("Error waiting for immediate fence for materials buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}
				}
				vmaDestroyBuffer(allocator, staging.buffer, staging.allocation);
			}

			gpu_meshes.reserve(a.meshes.size());
			size_t mesh_index{ 0 };
			for (const auto& [asset_positions, asset_indices, asset_surfaces, child_meshes] : a.meshes) {
				gpu_mesh_buffers gpu_mesh{};

				// *** SETTING VERTEX BUFFER *** //
				const size_t vertex_buffer_size = asset_positions.size() * sizeof(rosy_packager::position);
				{
					VkBufferCreateInfo buffer_info{};
					buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
					buffer_info.pNext = nullptr;
					buffer_info.size = vertex_buffer_size;
					buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

					VmaAllocationCreateInfo vma_alloc_info{};
					vma_alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
					vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

					if (
						const VkResult res = vmaCreateBuffer(
							allocator, &buffer_info, &vma_alloc_info, &gpu_mesh.vertex_buffer.buffer, &gpu_mesh.vertex_buffer.allocation,
							&gpu_mesh.vertex_buffer.info
						); res != VK_SUCCESS)
					{
						l->error(std::format("Error uploading vertex buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}
					gpu_mesh.graphics_created_bitmask |= graphics_created_bit_vertex_buffer;
					{
						const auto object_name = std::format("rosy vertex buffer {}", mesh_index);
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_BUFFER;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(gpu_mesh.vertex_buffer.buffer);
						debug_name.pObjectName = object_name.c_str();
						if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
						{
							l->error(std::format("Error creating vertex buffer name: {}", static_cast<uint8_t>(res)));
							return result::error;
						}
					}
					{
						VkBufferDeviceAddressInfo device_address_info{};
						device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
						device_address_info.buffer = gpu_mesh.vertex_buffer.buffer;

						// *** SETTING VERTEX BUFFER ADDRESS *** //
						gpu_mesh.vertex_buffer_address = vkGetBufferDeviceAddress(device, &device_address_info);
					}
				}

				// *** SETTING INDEX BUFFER *** //
				const size_t index_buffer_size = asset_indices.size() * sizeof(uint32_t);
				gpu_mesh.num_indices = static_cast<uint32_t>(asset_indices.size());
				{
					VkBufferCreateInfo buffer_info{};
					buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
					buffer_info.pNext = nullptr;
					buffer_info.size = index_buffer_size;
					buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

					VmaAllocationCreateInfo vma_alloc_info{};
					vma_alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
					vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

					if (
						const VkResult res = vmaCreateBuffer(
							allocator, &buffer_info, &vma_alloc_info, &gpu_mesh.index_buffer.buffer, &gpu_mesh.index_buffer.allocation,
							&gpu_mesh.index_buffer.info
						); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating index buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}
					{
						const auto object_name = std::format("rosy index buffer {}", mesh_index);
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_BUFFER;
						debug_name.objectHandle =  reinterpret_cast<uint64_t>(gpu_mesh.index_buffer.buffer);
						debug_name.pObjectName = object_name.c_str();
						if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
						{
							l->error(std::format("Error creating index buffer name: {}", static_cast<uint8_t>(res)));
							return result::error;
						}
						gpu_mesh.graphics_created_bitmask |= graphics_created_bit_index_buffer;
					}
				}

				// *** SETTING STAGING BUFFER *** //
				allocated_buffer staging{};
				{
					VkBufferCreateInfo buffer_info{};
					buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
					buffer_info.pNext = nullptr;
					buffer_info.size = vertex_buffer_size + index_buffer_size;
					buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

					VmaAllocationCreateInfo vma_alloc_info{};
					vma_alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
					vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

					if (const VkResult res = vmaCreateBuffer(allocator, &buffer_info, &vma_alloc_info, &staging.buffer, &staging.allocation, &staging.info); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating staging buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}
					{
						const auto object_name =  std::format("rosy vertex buffer staging {}", mesh_index);
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_BUFFER;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(staging.buffer);
						debug_name.pObjectName = object_name.c_str();
						if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
						{
							l->error(std::format("Error creating staging buffer name: {}", static_cast<uint8_t>(res)));
							return result::error;
						}
					}
				}

				memcpy(staging.info.pMappedData, asset_positions.data(), vertex_buffer_size);
				memcpy(static_cast<char*>(staging.info.pMappedData) + vertex_buffer_size, asset_indices.data(), index_buffer_size);

				if (VkResult res = vkResetFences(device, 1, &immediate_fence); res != VK_SUCCESS)
				{
					l->error(std::format("Error resetting immediate fence: {}", static_cast<uint8_t>(res)));
					return result::error;
				}

				if (VkResult res = vkResetCommandBuffer(immediate_command_buffer, 0); res != VK_SUCCESS)
				{
					l->error(std::format("Error resetting immediate command buffer: {}", static_cast<uint8_t>(res)));
					return result::error;
				}

				VkCommandBufferBeginInfo begin_info = {};
				begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

				if (VkResult res = vkBeginCommandBuffer(immediate_command_buffer, &begin_info); res != VK_SUCCESS)
				{
					l->error(std::format("Error beginning immediate command buffer: {}", static_cast<uint8_t>(res)));
					return result::error;
				}

				{
					VkBufferCopy vertex_copy{ 0 };
					vertex_copy.dstOffset = 0;
					vertex_copy.srcOffset = 0;
					vertex_copy.size = vertex_buffer_size;

					vkCmdCopyBuffer(immediate_command_buffer, staging.buffer, gpu_mesh.vertex_buffer.buffer, 1, &vertex_copy);

					VkBufferCopy index_copy{ 0 };
					index_copy.dstOffset = 0;
					index_copy.srcOffset = vertex_buffer_size;
					index_copy.size = index_buffer_size;

					vkCmdCopyBuffer(immediate_command_buffer, staging.buffer, gpu_mesh.index_buffer.buffer, 1, &index_copy);
				}

				if (VkResult res = vkEndCommandBuffer(immediate_command_buffer); res != VK_SUCCESS)
				{
					l->error(std::format("Error ending immediate command buffer: {}", static_cast<uint8_t>(res)));
					return result::error;
				}

				VkCommandBufferSubmitInfo cmd_buffer_submit_info = {};
				cmd_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
				cmd_buffer_submit_info.pNext = nullptr;
				cmd_buffer_submit_info.commandBuffer = immediate_command_buffer;
				cmd_buffer_submit_info.deviceMask = 0;
				VkSubmitInfo2 submit_info = {};
				submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
				submit_info.pNext = nullptr;

				submit_info.waitSemaphoreInfoCount = 0;
				submit_info.pWaitSemaphoreInfos = nullptr;
				submit_info.signalSemaphoreInfoCount = 0;
				submit_info.pSignalSemaphoreInfos = nullptr;
				submit_info.commandBufferInfoCount = 1;
				submit_info.pCommandBufferInfos = &cmd_buffer_submit_info;

				if (VkResult res = vkQueueSubmit2(present_queue, 1, &submit_info, immediate_fence); res != VK_SUCCESS)
				{
					l->error(std::format("Error submitting immediate command to present queue: {}", static_cast<uint8_t>(res)));
					return result::error;
				}

				if (VkResult res = vkWaitForFences(device, 1, &immediate_fence, true, 9999999999); res != VK_SUCCESS)
				{
					l->error(std::format("Error waiting for immediate fence: {}", static_cast<uint8_t>(res)));
					return result::error;
				}
				vmaDestroyBuffer(allocator, staging.buffer, staging.allocation);
				gpu_meshes.push_back(gpu_mesh);
				mesh_index += 1;
			}

			{
				if (a.shaders.empty())
				{
					l->error("No shader loaded");
					return result::error;
				}
				std::vector<VkShaderCreateInfoEXT> shader_create_info;
				const auto& [path, source] = a.shaders[0];

				if (source.empty())
				{
					l->error("No source in shader");
					return result::error;
				}

				std::vector<VkDescriptorSetLayout> layouts{};
				layouts.push_back(descriptor_set_layout);

				VkPushConstantRange push_constant_range{
					.stageFlags = VK_SHADER_STAGE_ALL,
					.offset = 0,
					.size = sizeof(gpu_draw_push_constants),
				};

				shader_create_info.push_back({
					.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
					.pNext = nullptr,
					.flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT,
					.stage = VK_SHADER_STAGE_VERTEX_BIT,
					.nextStage = VK_SHADER_STAGE_FRAGMENT_BIT,
					.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
					.codeSize = source.size(),
					.pCode = source.data(),
					.pName = "main",
					.setLayoutCount = static_cast<uint32_t>(layouts.size()),
					.pSetLayouts = layouts.data(),
					.pushConstantRangeCount = 1,
					.pPushConstantRanges = &push_constant_range,
					.pSpecializationInfo = nullptr,
					});
				shader_create_info.push_back({
					.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
					.pNext = nullptr,
					.flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT,
					.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
					.nextStage = 0,
					.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
					.codeSize = source.size(),
					.pCode = source.data(),
					.pName = "main",
					.setLayoutCount = static_cast<uint32_t>(layouts.size()),
					.pSetLayouts = layouts.data(),
					.pushConstantRangeCount = 1,
					.pPushConstantRanges = &push_constant_range,
					.pSpecializationInfo = nullptr,
					});

				scene_shaders.resize(shader_create_info.size());

				if (const VkResult res = vkCreateShadersEXT(device, static_cast<uint32_t>(shader_create_info.size()), shader_create_info.data(), nullptr, scene_shaders.data()); res != VK_SUCCESS) {
					l->error(std::format("Error creating shaders: {}", static_cast<uint8_t>(res)));
					return result::error;
				}
				graphics_created_bitmask |= graphics_created_bit_shaders;
				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_SHADER_EXT;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(scene_shaders.data()[0]);
					debug_name.pObjectName = "rosy vertex shader";
					if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating vertex shader name: {}", static_cast<uint8_t>(res)));
						return result::error;
					}
				}
				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_SHADER_EXT;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(scene_shaders.data()[1]);
					debug_name.pObjectName = "rosy fragment shader";
					if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating fragment shader name: {}", static_cast<uint8_t>(res)));
						return result::error;
					}
				}
				{
					VkPipelineLayoutCreateInfo pl_info{
						.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.setLayoutCount = static_cast<uint32_t>(layouts.size()),
						.pSetLayouts = layouts.data(),
						.pushConstantRangeCount = 1,
						.pPushConstantRanges = &push_constant_range,
					};
					if (const VkResult res = vkCreatePipelineLayout(device, &pl_info, nullptr, &scene_layout); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating shader pipeline layout: {}", static_cast<uint8_t>(res)));
						return result::error;
					}
					graphics_created_bitmask |= graphics_created_bit_pipeline_layout;
					{
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(scene_layout);
						debug_name.pObjectName = "rosy shader pipeline layout";
						if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
						{
							l->error(std::format("Error creating shader pipeline layout name: {}", static_cast<uint8_t>(res)));
							return result::error;
						}
					}
				}

			}
			return result::ok;
		}

		result set_graphic_objects(std::vector<graphics_object> graphics_objects)
		{
			surface_graphics.clear();
			std::vector<graphic_object_data> go_data{};
			go_data.reserve(graphics_objects.size());
			for (const auto& go : graphics_objects)
			{
				go_data.push_back({
				.transform = go.transform,
					});
				for (const auto& s : go.surface_data)
				{
					surface_graphics.push_back(s);
				}

			}

			// *** SETTING GRAPHICS OBJECTS BUFFER *** //

			{
				const size_t graphic_objects_buffer_size = go_data.size() * sizeof(graphic_object_data);
				{
					VkBufferCreateInfo buffer_info{};
					buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
					buffer_info.pNext = nullptr;
					buffer_info.size = graphic_objects_buffer_size;
					buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

					VmaAllocationCreateInfo vma_alloc_info{};
					vma_alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
					vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

					if (const VkResult res = vmaCreateBuffer(allocator, &buffer_info, &vma_alloc_info, &graphic_objects_buffer.go_buffer.buffer, &graphic_objects_buffer.go_buffer.allocation, &graphic_objects_buffer.go_buffer.info); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating graphics objects buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}
					{
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_BUFFER;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(graphic_objects_buffer.go_buffer.buffer);
						debug_name.pObjectName = "rosy graphics objects buffer";
						if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
						{
							l->error(std::format("Error creating graphics objects buffer name: {}", static_cast<uint8_t>(res)));
							return result::error;
						}
					}
				}

				{
					VkBufferDeviceAddressInfo device_address_info{};
					device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
					device_address_info.buffer = graphic_objects_buffer.go_buffer.buffer;

					// *** SETTING GRAPHICS OBJECTS BUFFER ADDRESS *** //
					graphic_objects_buffer.go_buffer_address = vkGetBufferDeviceAddress(device, &device_address_info);
				}

				allocated_buffer staging{};
				{
					VkBufferCreateInfo buffer_info{};
					buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
					buffer_info.pNext = nullptr;
					buffer_info.size = graphic_objects_buffer_size;
					buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

					VmaAllocationCreateInfo vma_alloc_info{};
					vma_alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
					vma_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

					if (const VkResult res = vmaCreateBuffer(allocator, &buffer_info, &vma_alloc_info, &staging.buffer, &staging.allocation, &staging.info); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating graphics objects staging buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}
					graphics_created_bitmask |= graphics_created_bit_graphics_buffer;
					{
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_BUFFER;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(staging.buffer);
						debug_name.pObjectName = "rosy material staging buffer";
						if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
						{
							l->error(std::format("Error creating graphics objects staging buffer name: {}", static_cast<uint8_t>(res)));
							return result::error;
						}
					}
				}

				memcpy(staging.info.pMappedData, go_data.data(), graphic_objects_buffer_size);

				{
					if (VkResult res = vkResetFences(device, 1, &immediate_fence); res != VK_SUCCESS)
					{
						l->error(std::format("Error resetting immediate fence for graphics objects buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}

					if (VkResult res =  vkResetCommandBuffer(immediate_command_buffer, 0); res != VK_SUCCESS)
					{
						l->error(std::format("Error resetting immediate command buffer for graphics objects buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}

					VkCommandBufferBeginInfo begin_info{};
					begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
					begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

					if (VkResult res = vkBeginCommandBuffer(immediate_command_buffer, &begin_info); res != VK_SUCCESS)
					{
						l->error(std::format("Error beginning immediate command buffer for graphics objects buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}

					VkBufferCopy vertex_copy{ 0 };
					vertex_copy.dstOffset = 0;
					vertex_copy.srcOffset = 0;
					vertex_copy.size = graphic_objects_buffer_size;

					vkCmdCopyBuffer(immediate_command_buffer, staging.buffer, graphic_objects_buffer.go_buffer.buffer, 1, &vertex_copy);

					if (VkResult res = vkEndCommandBuffer(immediate_command_buffer); res != VK_SUCCESS)
					{
						l->error(std::format("Error ending immediate command buffer for graphics objects buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}

					VkCommandBufferSubmitInfo cmd_buffer_submit_info{};
					cmd_buffer_submit_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
					cmd_buffer_submit_info.pNext = nullptr;
					cmd_buffer_submit_info.commandBuffer = immediate_command_buffer;
					cmd_buffer_submit_info.deviceMask = 0;
					VkSubmitInfo2 submit_info{};
					submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
					submit_info.pNext = nullptr;

					submit_info.waitSemaphoreInfoCount = 0;
					submit_info.pWaitSemaphoreInfos = nullptr;
					submit_info.signalSemaphoreInfoCount = 0;
					submit_info.pSignalSemaphoreInfos = nullptr;
					submit_info.commandBufferInfoCount = 1;
					submit_info.pCommandBufferInfos = &cmd_buffer_submit_info;

					if (VkResult res = vkQueueSubmit2(present_queue, 1, &submit_info, immediate_fence); res != VK_SUCCESS)
					{
						l->error(std::format("Error submitting staging buffer for graphics objects buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}

					if (VkResult res = vkWaitForFences(device, 1, &immediate_fence, true, 9999999999); res != VK_SUCCESS)
					{
						l->error(std::format("Error waiting for immediate fence for graphics objects buffer: {}", static_cast<uint8_t>(res)));
						return result::error;
					}
				}
				vmaDestroyBuffer(allocator, staging.buffer, staging.allocation);
			}

			return result::ok;
		}

		result render()
		{
			const auto start = std::chrono::system_clock::now();
			graphics_stats new_stats{};
			const frame_data cf = frame_datas[current_frame];

			if (const auto res = vkWaitForFences(device, 1, &cf.in_flight_fence, true, 1'000'000'000); res != VK_SUCCESS)
			{
				l->error(std::format("Error waiting for in-flight fence: {}", static_cast<uint8_t>(res)));
				return result::graphics_frame_failure;
			}

			if (const auto res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, cf.image_available_semaphore, VK_NULL_HANDLE, &swapchain_image_index); res != VK_SUCCESS)
			{
				l->error(std::format("Error waiting acquiring next image: {}", static_cast<uint8_t>(res)));
				return result::graphics_frame_failure;
			}

			VkImage swapchain_image = swapchain_images[swapchain_image_index];

			{
				draw_extent.width = std::min(swapchain_extent.width, draw_image.image_extent.width) * static_cast<uint32_t>(render_scale);
				draw_extent.height = std::min(swapchain_extent.height, draw_image.image_extent.height) * static_cast<uint32_t>(render_scale);
			}

			if (const auto res = vkResetFences(device, 1, &cf.in_flight_fence); res != VK_SUCCESS)
			{
				l->error(std::format("Error resetting in-flight fence: {}", static_cast<uint8_t>(res)));
				return result::graphics_frame_failure;
			}

			if (const auto res = vkResetCommandBuffer(cf.command_buffer, 0); res != VK_SUCCESS)
			{
				l->error(std::format("Error resetting command buffer: {}", static_cast<uint8_t>(res)));
				return result::graphics_frame_failure;
			}

			{
				constexpr VkCommandBufferBeginInfo begin_info{
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
					.pNext = nullptr,
					.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
					.pInheritanceInfo = nullptr,
				};
				if (const auto res = vkBeginCommandBuffer(cf.command_buffer, &begin_info); res != VK_SUCCESS)
				{
					l->error(std::format("Error begin recording command buffer: {}", static_cast<uint8_t>(res)));
					return result::graphics_frame_failure;
				}
				{
					VkDebugUtilsLabelEXT debug_label{};
					std::array<float, 4> debug_color{ 0.8f, 0.f, 0.f, 1.f };
					debug_label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
					debug_label.pNext = nullptr;
					debug_label.pLabelName = "render";
					std::copy_n(debug_color.data(), 4, debug_label.color);
					vkCmdBeginDebugUtilsLabelEXT(cf.command_buffer, &debug_label);
				}
				{
					VkDebugUtilsLabelEXT debug_label{};
					std::array<float, 4> debug_color{ 0.7f, 0.f, 0.f, 1.f };
					debug_label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
					debug_label.pNext = nullptr;
					debug_label.pLabelName = "update scene buffer";
					std::copy_n(debug_color.data(), 4, debug_label.color);
					vkCmdBeginDebugUtilsLabelEXT(cf.command_buffer, &debug_label);
				}
				{
					// Update scene buffer
					vkCmdUpdateBuffer(cf.command_buffer, scene_buffer.scene_buffer.buffer, 0, sizeof(gpu_scene_data), &scene_data);
				}
				vkCmdEndDebugUtilsLabelEXT(cf.command_buffer);
			}
			{
				VkDebugUtilsLabelEXT debug_label{};
				std::array<float, 4> debug_color{ 0.6f, 0.f, 0.f, 1.f };
				debug_label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
				debug_label.pNext = nullptr;
				debug_label.pLabelName = "clear image";
				std::copy_n(debug_color.data(), 4, debug_label.color);
				vkCmdBeginDebugUtilsLabelEXT(cf.command_buffer, &debug_label);
			}
			{
				{
					constexpr VkImageSubresourceRange subresource_range{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = VK_REMAINING_MIP_LEVELS,
						.baseArrayLayer = 0,
						.layerCount = VK_REMAINING_ARRAY_LAYERS,
					};

					VkImageMemoryBarrier2 image_barrier = {
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.pNext = nullptr,
						.srcStageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
						.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
						.dstStageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
						.dstAccessMask =  VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_GENERAL,
						.srcQueueFamilyIndex = 0,
						.dstQueueFamilyIndex = 0,
						.image = draw_image.image,
						.subresourceRange = subresource_range,
					};

					const VkDependencyInfo dependency_info{
						.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
						.pNext = nullptr,
						.dependencyFlags = 0,
						.memoryBarrierCount = 0,
						.pMemoryBarriers = nullptr,
						.bufferMemoryBarrierCount = 0,
						.pBufferMemoryBarriers = nullptr,
						.imageMemoryBarrierCount = 1,
						.pImageMemoryBarriers = &image_barrier,
					};

					vkCmdPipelineBarrier2(cf.command_buffer, &dependency_info);
				}

				{
					constexpr VkImageSubresourceRange subresource_range{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = VK_REMAINING_MIP_LEVELS,
						.baseArrayLayer = 0,
						.layerCount = VK_REMAINING_ARRAY_LAYERS,
					};

					VkClearColorValue clear_value;
					clear_value = { {0.0f, 0.05f, 0.1f, 1.0f} };
					vkCmdClearColorImage(cf.command_buffer, draw_image.image, VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1, &subresource_range);
				}
				vkCmdEndDebugUtilsLabelEXT(cf.command_buffer);
			}

			{
				VkDebugUtilsLabelEXT debug_label{};
				std::array<float, 4> debug_color{ 0.5f, 0.f, 0.f, 1.f };
				debug_label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
				debug_label.pNext = nullptr;
				debug_label.pLabelName = "prepare rendering";
				std::copy_n(debug_color.data(), 4, debug_label.color);
				vkCmdBeginDebugUtilsLabelEXT(cf.command_buffer, &debug_label);
			}
			{
				{
					constexpr VkImageSubresourceRange subresource_range{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = VK_REMAINING_MIP_LEVELS,
						.baseArrayLayer = 0,
						.layerCount = VK_REMAINING_ARRAY_LAYERS,
					};

					VkImageMemoryBarrier2 image_barrier = {
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.pNext = nullptr,
						.srcStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
						.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
						.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						.dstAccessMask =  VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
						.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.srcQueueFamilyIndex = 0,
						.dstQueueFamilyIndex = 0,
						.image = draw_image.image,
						.subresourceRange = subresource_range,
					};

					const VkDependencyInfo dependency_info{
						.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
						.pNext = nullptr,
						.dependencyFlags = 0,
						.memoryBarrierCount = 0,
						.pMemoryBarriers = nullptr,
						.bufferMemoryBarrierCount = 0,
						.pBufferMemoryBarriers = nullptr,
						.imageMemoryBarrierCount = 1,
						.pImageMemoryBarriers = &image_barrier,
					};

					vkCmdPipelineBarrier2(cf.command_buffer, &dependency_info);
				}
				{
					constexpr VkImageSubresourceRange subresource_range{
						.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
						.baseMipLevel = 0,
						.levelCount = VK_REMAINING_MIP_LEVELS,
						.baseArrayLayer = 0,
						.layerCount = VK_REMAINING_ARRAY_LAYERS,
					};

					VkImageMemoryBarrier2 image_barrier = {
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.pNext = nullptr,
						.srcStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
						.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
						.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						.dstAccessMask =  VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
						.srcQueueFamilyIndex = 0,
						.dstQueueFamilyIndex = 0,
						.image = depth_image.image,
						.subresourceRange = subresource_range,
					};

					const VkDependencyInfo dependency_info{
						.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
						.pNext = nullptr,
						.dependencyFlags = 0,
						.memoryBarrierCount = 0,
						.pMemoryBarriers = nullptr,
						.bufferMemoryBarrierCount = 0,
						.pBufferMemoryBarriers = nullptr,
						.imageMemoryBarrierCount = 1,
						.pImageMemoryBarriers = &image_barrier,
					};

					vkCmdPipelineBarrier2(cf.command_buffer, &dependency_info);
				}
				vkCmdEndDebugUtilsLabelEXT(cf.command_buffer);

				{
					VkDebugUtilsLabelEXT debug_label{};
					std::array<float, 4> debug_color{ 0.4f, 0.f, 0.f, 1.f };
					debug_label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
					debug_label.pNext = nullptr;
					debug_label.pLabelName = "rendering";
					std::copy_n(debug_color.data(), 4, debug_label.color);
					vkCmdBeginDebugUtilsLabelEXT(cf.command_buffer, &debug_label);
				}
				{
					{
						VkRenderingAttachmentInfo color_attachment{};
						color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
						color_attachment.pNext = nullptr;
						color_attachment.imageView = draw_image.image_view;
						color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
						color_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
						color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
						color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;


						VkRenderingAttachmentInfo depth_attachment{};
						depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
						depth_attachment.pNext = nullptr;
						depth_attachment.imageView = depth_image.image_view;
						depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
						depth_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
						depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
						depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
						depth_attachment.clearValue.depthStencil.depth = 0.0f;

						const auto render_area = VkRect2D{ VkOffset2D{0, 0}, swapchain_extent };
						VkRenderingInfo render_info{};
						render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
						render_info.pNext = nullptr;
						render_info.renderArea = render_area;
						render_info.layerCount = 1;
						render_info.colorAttachmentCount = 1;
						render_info.pColorAttachments = &color_attachment;
						render_info.pDepthAttachment = &depth_attachment;
						render_info.pStencilAttachment = nullptr;

						vkCmdBeginRendering(cf.command_buffer, &render_info);
					}
					{
						{
							vkCmdSetRasterizerDiscardEnableEXT(cf.command_buffer, VK_FALSE);
							vkCmdSetPrimitiveTopologyEXT(cf.command_buffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
							vkCmdSetPrimitiveRestartEnableEXT(cf.command_buffer, VK_FALSE);
							vkCmdSetRasterizationSamplesEXT(cf.command_buffer, VK_SAMPLE_COUNT_1_BIT);
						}
						{
							constexpr VkSampleMask sample_mask = 0x1;
							vkCmdSetSampleMaskEXT(cf.command_buffer, VK_SAMPLE_COUNT_1_BIT, &sample_mask);
						}
						{
							VkColorComponentFlags color_component_flags[] = { VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_A_BIT };
							vkCmdSetColorWriteMaskEXT(cf.command_buffer, 0, 1, color_component_flags);
						}
						{
							vkCmdSetVertexInputEXT(cf.command_buffer, 0, nullptr, 0, nullptr);
						}

						{

							vkCmdSetFrontFaceEXT(cf.command_buffer, VK_FRONT_FACE_CLOCKWISE);
							vkCmdSetCullModeEXT(cf.command_buffer, VK_CULL_MODE_FRONT_BIT);
							vkCmdSetPolygonModeEXT(cf.command_buffer, VK_POLYGON_MODE_FILL);
						}

						{
							VkViewport viewport{};
							viewport.x = 0.0f;
							viewport.y = 0.0f;
							viewport.width = static_cast<float>(swapchain_extent.width);
							viewport.height = static_cast<float>(swapchain_extent.height);
							viewport.minDepth = 0.0f;
							viewport.maxDepth = 1.0f;
							vkCmdSetViewport(cf.command_buffer, 0, 1, &viewport);
							vkCmdSetViewportWithCountEXT(cf.command_buffer, 1, &viewport);
						}
						{
							VkRect2D scissor{};
							scissor.offset = { 0, 0 };
							scissor.extent = swapchain_extent;
							vkCmdSetScissor(cf.command_buffer, 0, 1, &scissor);
							vkCmdSetScissorWithCountEXT(cf.command_buffer, 1, &scissor);
						}
						{

							vkCmdSetDepthTestEnableEXT(cf.command_buffer, VK_TRUE);
							vkCmdSetDepthWriteEnableEXT(cf.command_buffer, VK_TRUE);
							vkCmdSetDepthCompareOpEXT(cf.command_buffer, VK_COMPARE_OP_GREATER_OR_EQUAL);
							vkCmdSetDepthBoundsTestEnableEXT(cf.command_buffer, VK_FALSE);
							vkCmdSetDepthBiasEnableEXT(cf.command_buffer, VK_FALSE);
							vkCmdSetStencilTestEnableEXT(cf.command_buffer, VK_FALSE);
							vkCmdSetDepthClipEnableEXT(cf.command_buffer, VK_FALSE);
							vkCmdSetDepthClampEnableEXT(cf.command_buffer, VK_FALSE);
							vkCmdSetLogicOpEnableEXT(cf.command_buffer, VK_FALSE);
							vkCmdSetDepthBounds(cf.command_buffer, 0.0f, 1.0f);
							vkCmdSetAlphaToCoverageEnableEXT(cf.command_buffer, VK_FALSE);
						}
						{
							constexpr auto enable = VK_FALSE;
							vkCmdSetColorBlendEnableEXT(cf.command_buffer, 0, 1, &enable);
						}
					}
					{
						constexpr VkShaderStageFlagBits stages[2] =
						{
							VK_SHADER_STAGE_VERTEX_BIT,
							VK_SHADER_STAGE_FRAGMENT_BIT
						};
						vkCmdBindShadersEXT(cf.command_buffer, 2, stages, scene_shaders.data());
						constexpr VkShaderStageFlagBits unused_stages[4] =
						{
							VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
							VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
							VK_SHADER_STAGE_GEOMETRY_BIT,
						};
						vkCmdBindShadersEXT(cf.command_buffer, 3, unused_stages, nullptr);
						vkCmdBindDescriptorSets(cf.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, scene_layout, 0, 1, &descriptor_set, 0, nullptr);
						{
							size_t current_mesh_index = UINT64_MAX;
							for (auto& surface_graphic : surface_graphics)
							{
								auto& gpu_mesh = gpu_meshes[surface_graphic.mesh_index];
								if (surface_graphic.mesh_index != current_mesh_index)
								{
									vkCmdBindIndexBuffer(cf.command_buffer, gpu_mesh.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
									current_mesh_index = surface_graphic.mesh_index;
								}
								gpu_draw_push_constants pc{
									.scene_buffer = scene_buffer.scene_buffer_address,
									.vertex_buffer = gpu_mesh.vertex_buffer_address,
									.go_buffer = graphic_objects_buffer.go_buffer_address + (sizeof(graphic_object_data) * surface_graphic.graphics_object_index),
									.material_buffer = material_buffer.material_buffer_address + (sizeof(gpu_material) * surface_graphic.material_index),
								};
								vkCmdPushConstants(cf.command_buffer, scene_layout, VK_SHADER_STAGE_ALL, 0, sizeof(gpu_draw_push_constants), &pc);
								vkCmdDrawIndexed(cf.command_buffer, surface_graphic.index_count, 1, surface_graphic.start_index, 0, 0);
								new_stats.draw_call_count += 1;
								new_stats.triangle_count += surface_graphic.index_count / 3;
							}
						}
					}
					if (render_ui) {
						ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cf.command_buffer);
					}
					vkCmdEndRendering(cf.command_buffer);
					vkCmdEndDebugUtilsLabelEXT(cf.command_buffer);
				}
			}
			{
				VkDebugUtilsLabelEXT debug_label{};
				std::array<float, 4> debug_color{ 0.3f, 0.f, 0.f, 1.f };
				debug_label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
				debug_label.pNext = nullptr;
				debug_label.pLabelName = "blit";
				std::copy_n(debug_color.data(), 4, debug_label.color);
				vkCmdBeginDebugUtilsLabelEXT(cf.command_buffer, &debug_label);
			}
			{
				{
					constexpr VkImageSubresourceRange subresource_range{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = VK_REMAINING_MIP_LEVELS,
						.baseArrayLayer = 0,
						.layerCount = VK_REMAINING_ARRAY_LAYERS,
					};

					VkImageMemoryBarrier2 image_barrier = {
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.pNext = nullptr,
						.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
						.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						.dstAccessMask =  VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						.srcQueueFamilyIndex = 0,
						.dstQueueFamilyIndex = 0,
						.image = draw_image.image,
						.subresourceRange = subresource_range,
					};

					const VkDependencyInfo dependency_info{
						.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
						.pNext = nullptr,
						.dependencyFlags = 0,
						.memoryBarrierCount = 0,
						.pMemoryBarriers = nullptr,
						.bufferMemoryBarrierCount = 0,
						.pBufferMemoryBarriers = nullptr,
						.imageMemoryBarrierCount = 1,
						.pImageMemoryBarriers = &image_barrier,
					};

					vkCmdPipelineBarrier2(cf.command_buffer, &dependency_info);
				}

				{
					constexpr VkImageSubresourceRange subresource_range{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = VK_REMAINING_MIP_LEVELS,
						.baseArrayLayer = 0,
						.layerCount = VK_REMAINING_ARRAY_LAYERS,
					};

					VkImageMemoryBarrier2 image_barrier = {
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
						.pNext = nullptr,
						.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
						.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
						.dstAccessMask =  VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.srcQueueFamilyIndex = 0,
						.dstQueueFamilyIndex = 0,
						.image = swapchain_image,
						.subresourceRange = subresource_range,
					};

					const VkDependencyInfo dependency_info{
						.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
						.pNext = nullptr,
						.dependencyFlags = 0,
						.memoryBarrierCount = 0,
						.pMemoryBarriers = nullptr,
						.bufferMemoryBarrierCount = 0,
						.pBufferMemoryBarriers = nullptr,
						.imageMemoryBarrierCount = 1,
						.pImageMemoryBarriers = &image_barrier,
					};

					vkCmdPipelineBarrier2(cf.command_buffer, &dependency_info);
				}

				{
					VkImageBlit2 blit_region = {};
					blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
					blit_region.pNext = nullptr;
					blit_region.srcOffsets[1].x = static_cast<int32_t>(draw_extent.width);
					blit_region.srcOffsets[1].y = static_cast<int32_t>(draw_extent.height);
					blit_region.srcOffsets[1].z = 1;

					blit_region.dstOffsets[1].x = static_cast<int32_t>(swapchain_extent.width);
					blit_region.dstOffsets[1].y = static_cast<int32_t>(swapchain_extent.height);
					blit_region.dstOffsets[1].z = 1;

					blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					blit_region.srcSubresource.baseArrayLayer = 0;
					blit_region.srcSubresource.layerCount = 1;
					blit_region.srcSubresource.mipLevel = 0;

					blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					blit_region.dstSubresource.baseArrayLayer = 0;
					blit_region.dstSubresource.layerCount = 1;
					blit_region.dstSubresource.mipLevel = 0;

					VkBlitImageInfo2 blit_info{
						.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
						.pNext = nullptr
					};
					blit_info.dstImage = swapchain_image;
					blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
					blit_info.srcImage = draw_image.image;
					blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
					blit_info.filter = VK_FILTER_LINEAR;
					blit_info.regionCount = 1;
					blit_info.pRegions = &blit_region;

					vkCmdBlitImage2(cf.command_buffer, &blit_info);
					vkCmdEndDebugUtilsLabelEXT(cf.command_buffer);
				}
			}
			{
				VkDebugUtilsLabelEXT debug_label{};
				std::array<float, 4> debug_color{ 0.2f, 0.f, 0.f, 1.f };
				debug_label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
				debug_label.pNext = nullptr;
				debug_label.pLabelName = "prepare present";
				std::copy_n(debug_color.data(), 4, debug_label.color);
				vkCmdBeginDebugUtilsLabelEXT(cf.command_buffer, &debug_label);
			}
			{
				constexpr VkImageSubresourceRange subresource_range{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = VK_REMAINING_MIP_LEVELS,
					.baseArrayLayer = 0,
					.layerCount = VK_REMAINING_ARRAY_LAYERS,
				};

				VkImageMemoryBarrier2 image_barrier = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.pNext = nullptr,
					.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					.dstAccessMask =  VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					.srcQueueFamilyIndex = 0,
					.dstQueueFamilyIndex = 0,
					.image = swapchain_image,
					.subresourceRange = subresource_range,
				};

				const VkDependencyInfo dependency_info{
					.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
					.pNext = nullptr,
					.dependencyFlags = 0,
					.memoryBarrierCount = 0,
					.pMemoryBarriers = nullptr,
					.bufferMemoryBarrierCount = 0,
					.pBufferMemoryBarriers = nullptr,
					.imageMemoryBarrierCount = 1,
					.pImageMemoryBarriers = &image_barrier,
				};

				vkCmdPipelineBarrier2(cf.command_buffer, &dependency_info);
			}
			vkCmdEndDebugUtilsLabelEXT(cf.command_buffer);
			vkCmdEndDebugUtilsLabelEXT(cf.command_buffer);
			if (const auto res = vkEndCommandBuffer(cf.command_buffer); res != VK_SUCCESS)
			{
				l->error(std::format("Error ending command buffer recording: {}", static_cast<uint8_t>(res)));
				return result::graphics_frame_failure;
			}

			{
				VkCommandBufferSubmitInfo cmd_buffer_submit_info{
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
					.pNext = nullptr,
					.commandBuffer = cf.command_buffer,
					.deviceMask = 0,
				};

				VkSemaphoreSubmitInfo wait_info{
					.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
					.pNext = nullptr,
					.semaphore = cf.image_available_semaphore,
					.value = 1,
					.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
					.deviceIndex = 0,
				};

				VkSemaphoreSubmitInfo signal_info{
					.sType= VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
					.pNext= nullptr,
					.semaphore= cf.render_finished_semaphore,
					.value= 1,
					.stageMask= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
					.deviceIndex= 0,
				};

				VkSubmitInfo2 submit_info{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
					.pNext = nullptr,
					.flags = 0,
					.waitSemaphoreInfoCount = 1,
					.pWaitSemaphoreInfos = &wait_info,
					.commandBufferInfoCount = 1,
					.pCommandBufferInfos = &cmd_buffer_submit_info,
					.signalSemaphoreInfoCount = 1,
					.pSignalSemaphoreInfos = &signal_info,
				};

				if (const auto res = vkQueueSubmit2(present_queue, 1, &submit_info, cf.in_flight_fence); res != VK_SUCCESS)
				{
					l->error(std::format("Error submitting to present queue: {}", static_cast<uint8_t>(res)));
					return result::graphics_frame_failure;
				}
			}

			{
				VkSwapchainKHR swap_chains[] = { swapchain };
				VkPresentInfoKHR present_info{
					.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
					.pNext = nullptr,
					.waitSemaphoreCount = 1,
					.pWaitSemaphores = &cf.render_finished_semaphore,
					.swapchainCount = 1,
					.pSwapchains = swap_chains,
					.pImageIndices = &swapchain_image_index,
					.pResults = nullptr,
				};

				if (const auto res = vkQueuePresentKHR(present_queue, &present_info); res != VK_SUCCESS)
				{
					l->error(std::format("Error presenting to swapchain: {}", static_cast<uint8_t>(res)));
					return result::graphics_frame_failure;
				}
			}

			current_frame = (current_frame + 1) % swapchain_image_count;

			const auto end = std::chrono::system_clock::now();
			const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
			new_stats.draw_time = elapsed.count() / 1000.f;
			stats = new_stats;
			return result::ok;
		}

		result update(const gpu_scene_data& sd)
		{
			scene_data = sd;
			return result::ok;
		}

		result ui(const engine_stats& eng_stats)
		{
			ImGui::ShowDemoWindow();

			ImGuiWindowFlags window_flags{ 0 };
			window_flags |= ImGuiWindowFlags_NoCollapse;
			if (ImGui::Begin("Game State", nullptr, window_flags))
			{
				if (constexpr ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None; ImGui::BeginTabBar("ViewEdit", tab_bar_flags))
				{
					if (ImGui::BeginTabItem("View"))
					{
						if (ImGui::BeginTable("Scene Data", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
						{
							ImGui::TableNextRow();
							ImGui::TableNextColumn();
							ImGui::Text("Camera position");
							ImGui::TableNextColumn();
							ImGui::Text("(%.2f,  %.2f,  %.2f)", scene_data.camera_position[0], scene_data.camera_position[1], scene_data.camera_position[2]);
							ImGui::TableNextRow();
							ImGui::TableNextColumn();
							ImGui::Text("Light direction");
							ImGui::TableNextColumn();
							ImGui::Text("(%.2f,  %.2f,  %.2f)", scene_data.sunlight[0], scene_data.sunlight[1], scene_data.sunlight[2]);
							ImGui::EndTable();
						}
						ImGui::EndTabItem();
					}
					if (ImGui::BeginTabItem("Edit"))
					{
						if (ImGui::BeginTable("Scene Data", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
						{
							ImGui::TableNextRow();
							ImGui::TableNextColumn();
							ImGui::Text("Camera position");
							ImGui::TableNextColumn();
							ImGui::Text("(%.2f,  %.2f,  %.2f)", scene_data.camera_position[0], scene_data.camera_position[1], scene_data.camera_position[2]);
							ImGui::EndTable();
						}
						if (ImGui::BeginTable("Edit Scene Data", 1, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
						{
							ImGui::TableNextRow();
							ImGui::TableNextColumn();
							ImGui::SliderFloat3("Light direction", wls->light, -2 * static_cast<float>(pi), 2 * static_cast<float>(pi));
							ImGui::EndTable();
						}
						ImGui::EndTabItem();
					}
					ImGui::EndTabBar();
				}
			
				ImGui::NewLine();
				if (ImGui::BeginTable("Performance", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders))
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("a_fps");
					ImGui::TableNextColumn();
					ImGui::Text("%.0f rad/s", eng_stats.a_fps);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("d_fps");
					ImGui::TableNextColumn();
					ImGui::Text("%.0f °/s", eng_stats.d_fps);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("r_fps");
					ImGui::TableNextColumn();
					ImGui::Text("%.0f", eng_stats.r_fps);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("frame time");
					ImGui::TableNextColumn();
					ImGui::Text("%.3fms", eng_stats.frame_time);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("update time");
					ImGui::TableNextColumn();
					ImGui::Text("%.3f ms", eng_stats.level_update_time);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("draw time");
					ImGui::TableNextColumn();
					ImGui::Text("%.3f ms", stats.draw_time);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("triangles");
					ImGui::TableNextColumn();
					ImGui::Text("%i", stats.triangle_count);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("lines");
					ImGui::TableNextColumn();
					ImGui::Text("%i", stats.line_count);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("draws ");
					ImGui::TableNextColumn();
					ImGui::Text("%i", stats.draw_call_count);

					ImGui::EndTable();
				}
			}
			ImGui::End();

			return result::ok;
		}
	};

	graphics_device* gd{ nullptr };
}


//// Graphics

result graphics::init(SDL_Window* new_window, log const* new_log, config cfg)
{
	if (new_window == nullptr)
	{
		return result::invalid_argument;
	}
	if (new_log == nullptr)
	{
		return result::invalid_argument;
	}
	{
		l = new_log;
		l->info("Graphics init start");
	}
	{
		// Init graphics device
		gd = new(std::nothrow) graphics_device;
		if (gd == nullptr)
		{
			l->error("graphics_device allocation failed");
			return result::allocation_failure;
		}
		gd->l = new_log;
		gd->window = new_window;
		if (const auto res = gd->init(cfg); res != result::ok)
		{
			l->error("graphics_device initialization failed");
			return result::graphics_init_failure;
		}
	}

	viewport_width = gd->swapchain_extent.width;
	viewport_height = gd->swapchain_extent.height;
	l->info("Graphics init done");

	return result::ok;
}

void graphics::deinit()
{
	l->info("Graphics deinit start");

	if (gd)
	{
		gd->deinit();
		delete gd;
		gd = nullptr;
	}

	l->info("Graphics deinit end");
	l = nullptr;
}

result graphics::set_asset(const rosy_packager::asset& a, const std::vector<graphics_object>& graphics_objects, write_level_state* wls) const
{
	l->debug(std::format("Setting asset with {} graphic objects.", graphics_objects.size()));
	gd->wls = wls; // Set writable state, this is a pointer to level data that the UI can write to.
	if (const auto res = gd->set_asset(a); res != result::ok)
	{
		return res;
	}
	if (const auto res = gd->set_graphic_objects(graphics_objects); res != result::ok)
	{
		return res;
	}
	return result::ok;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
result graphics::update(const read_level_state& rls)
{
	const gpu_scene_data sd{
		.view = rls.v,
		.proj = rls.p,
		.view_projection = rls.vp,
		.sunlight = { 0.25f, 0.98f, 0.1f },
		.camera_position = rls.cam_pos,
		.ambient_color = { 0.11f,  0.11f, 0.11f, 1.f, },
		.sunlight_color = { 0.55f, 0.55f, 0.55f, 1.f },
	};
	if (const auto res = gd->update(sd); res != result::ok)
	{
		return res;
	}
	return result::ok;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
result graphics::render(const bool render_ui, const engine_stats& stats)
{
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
	}
	{
		if (const auto res = gd->ui(stats); res != result::ok)
		{
			return res;
		}
		ImGui::Render();
	}
	gd->render_ui = render_ui;
	return gd->render();
}

result graphics::resize()
{
	if (const auto res = gd->resize_swapchain(); res != result::ok)
	{
		return res;
	}

	viewport_width = gd->swapchain_extent.width;
	viewport_height = gd->swapchain_extent.height;

	return result::ok;
}

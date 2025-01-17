#include "Graphics.h"
#include <format>
#include <vector>
#include <queue>
#include <stack>

#include "volk/volk.h"
#include "vma/vk_mem_alloc.h"
#include <SDL3/SDL_vulkan.h>

#pragma warning(disable: 4100 4459)
#include <algorithm>
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>
#pragma warning(default: 4100 4459)
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

	constexpr  uint32_t graphics_created_bit_instance         = 0b00000000000000000000000000000001;
	constexpr  uint32_t graphics_created_bit_device	          = 0b00000000000000000000000000000010;
	constexpr  uint32_t graphics_created_bit_surface          = 0b00000000000000000000000000000100;
	constexpr  uint32_t graphics_created_bit_vma              = 0b00000000000000000000000000001000;
	constexpr  uint32_t graphics_created_bit_debug_messenger  = 0b00000000000000000000000000010000;
	constexpr  uint32_t graphics_created_bit_fence            = 0b00000000000000000000000000100000;
	constexpr  uint32_t graphics_created_bit_command_pool     = 0b00000000000000000000000001000000;
	constexpr  uint32_t graphics_created_bit_draw_image       = 0b00000000000000000000000010000000;
	constexpr  uint32_t graphics_created_bit_depth_image      = 0b00000000000000000000000100000000;
	constexpr  uint32_t graphics_created_bit_ui_pool          = 0b00000000000000000000001000000000;
	constexpr  uint32_t graphics_created_bit_swapchain        = 0b00000000000000000000010000000000;
	constexpr  uint32_t graphics_created_bit_ktx              = 0b00000000000000000000100000000000;
	constexpr  uint32_t graphics_created_bit_descriptor_set   = 0b00000000000000000001000000000000;
	constexpr  uint32_t graphics_created_bit_descriptor_pool  = 0b00000000000000000010000000000000;
	constexpr  uint32_t graphics_created_bit_draw_image_view  = 0b00000000000000000100000000000000;
	constexpr  uint32_t graphics_created_bit_depth_image_view = 0b00000000000000001000000000000000;
	constexpr  uint32_t graphics_created_bit_image_semaphore  = 0b00000000000000010000000000000000;
	constexpr  uint32_t graphics_created_bit_pass_semaphore   = 0b00000000000000100000000000000000;
	constexpr  uint32_t graphics_created_bit_imgui_sdl        = 0b00000000000001000000000000000000;
	constexpr  uint32_t graphics_created_bit_imgui_vk         = 0b00000000000010000000000000000000;
	constexpr  uint32_t graphics_created_bit_imgui_ctx        = 0b00000000000100000000000000000000;
	constexpr  uint32_t graphics_created_bit_vertex_buffer    = 0b00000000001000000000000000000000;
	constexpr  uint32_t graphics_created_bit_index_buffer     = 0b00000000010000000000000000000000;
	constexpr  uint32_t graphics_created_bit_shaders          = 0b00000000100000000000000000000000;
	constexpr  uint32_t graphics_created_bit_pipeline_layout  = 0b00000001000000000000000000000000;

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
		//VK_KHR_MULTIVIEW_EXTENSION_NAME,
	};

	constexpr uint8_t max_frames_in_flight = 2;

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
		uint32_t frame_graphics_created_bitmask{ 0 };
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

	struct allocated_image
	{
		VkImage image;
		VkImageView image_view;
		VmaAllocation allocation;
		VkExtent3D image_extent;
		VkFormat image_format;
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

	struct gpu_mesh_buffers
	{
		uint32_t graphics_created_bitmask{ 0 };
		allocated_buffer index_buffer;
		allocated_buffer vertex_buffer;
		VkDeviceAddress vertex_buffer_address;
		std::vector<VkShaderEXT> shaders;
		VkPipelineLayout layout;
	};

	struct gpu_draw_push_constants
	{
		//VkDeviceAddress scene_buffer{ 0 };
		VkDeviceAddress vertex_buffer{ 0 };
		//VkDeviceAddress render_buffer{ 0 };
		//VkDeviceAddress material_buffer{ 0 };
	};

	struct graphics_device
	{
		rosy::log const* l{ nullptr };
		config cfg{};
		uint32_t graphics_created_bitmask{ 0 };
		bool enable_validation_layers{ true };

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
		float render_scale = 1.f;

		VkDescriptorPool ui_pool{ nullptr };

		VkFence immediate_fence{ nullptr };
		VkCommandBuffer immediate_command_buffer{ nullptr };
		VkCommandPool immediate_command_pool{ nullptr };

		SDL_Window* window{ nullptr };

		// Buffers
		gpu_mesh_buffers gpu_mesh{};
		allocated_buffer scene_buffer{};
		allocated_buffer material_buffer{};
		allocated_buffer surface_buffer{};

		result init(const config new_cfg)
		{
			cfg = new_cfg;
			VkResult vk_result = volkInitialize();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to initialize volk! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = query_instance_layers();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to query instance layers! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = query_instance_extensions();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to query instance extensions! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_instance();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to create Vulkan instance! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = create_debug_callback();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to create Vulkan debug callback! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_surface();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to create surface! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_physical_device();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to create Vulkan physical device! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = query_device_layers();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to query device layers! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = query_device_extensions();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to query device extensions! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_device();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to create Vulkan device {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_tracy();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init tracy! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_allocator();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init vma! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_presentation_queue();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to get presentation queue! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_swapchain();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init swap chain! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_swapchain_failure;
			}

			vk_result = init_draw_image();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init draw image! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_descriptors();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init draw descriptors! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_command_pool();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init command pool! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_command_buffers();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init command buffers! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_sync_objects();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init sync objects! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_ui();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init UI! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_csm_image();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init csm! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_commands();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init commands! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_data();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init data! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}

			vk_result = init_ktx();
			if (vk_result != VK_SUCCESS)
			{
				l->error(std::format("Failed to init ktx! {}", static_cast<uint8_t>(vk_result)));
				return result::graphics_init_failure;
			}
			return result::ok;
		}

		void deinit()
		{
			// Deinit acquired resources in the opposite order in which they were created

			if (gpu_mesh.graphics_created_bitmask & graphics_created_bit_pipeline_layout)
			{
				vkDestroyPipelineLayout(device, gpu_mesh.layout, nullptr);
			}

			if (gpu_mesh.graphics_created_bitmask & graphics_created_bit_shaders)
			{
				for (const VkShaderEXT shader : gpu_mesh.shaders)
				{
					vkDestroyShaderEXT(device, shader, nullptr);
				}
			}

			if (gpu_mesh.graphics_created_bitmask & graphics_created_bit_index_buffer)
			{
				vmaDestroyBuffer(allocator, gpu_mesh.index_buffer.buffer, gpu_mesh.index_buffer.allocation);
			}

			if (gpu_mesh.graphics_created_bitmask & graphics_created_bit_vertex_buffer)
			{
				vmaDestroyBuffer(allocator, gpu_mesh.vertex_buffer.buffer, gpu_mesh.vertex_buffer.allocation);
			}

			if (graphics_created_bitmask & graphics_created_bit_device)
			{
				if (const VkResult result = vkDeviceWaitIdle(device); result != VK_SUCCESS)
				{
					l->error(std::format("Failed to wait device to be idle: {}", static_cast<uint8_t>(result)));
				}
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
				if (const VkResult result = vkDeviceWaitIdle(device); result == VK_SUCCESS) vkDestroyDevice(device, nullptr);
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
			VkResult result;

			uint32_t p_property_count = 0;
			{
				if (result = vkEnumerateInstanceLayerProperties(&p_property_count, nullptr); result != VK_SUCCESS) return result;
				l->debug(std::format("Found %d instance layers {}", p_property_count));
				if (p_property_count == 0) return result;
			}

			std::vector<VkLayerProperties> layers;
			{
				layers.resize(p_property_count);
				if (result = vkEnumerateInstanceLayerProperties(&p_property_count, layers.data()); result != VK_SUCCESS) return result;
				if (!enable_validation_layers) return result;
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
			return result;
		}

		VkResult query_instance_extensions()
		{
			l->info("Querying instance extensions");
			VkResult result;

			uint32_t p_property_count = 0;
			{
				if (result = vkEnumerateInstanceExtensionProperties(nullptr, &p_property_count, nullptr); result != VK_SUCCESS) return result;
				l->debug(std::format("Found {} instance extensions", p_property_count));
				if (p_property_count == 0) return result;
			}

			std::vector<VkExtensionProperties> extensions;
			{
				extensions.resize(p_property_count);
				if (result = vkEnumerateInstanceExtensionProperties(nullptr, &p_property_count, extensions.data()); result != VK_SUCCESS) return result;
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

			// ReSharper disable once CppUseStructuredBinding
			for (VkExtensionProperties properties : extensions)
			{
				l->debug(std::format("Instance extension name: {}", properties.extensionName));
				for (const char* extension_name : instance_extensions)
				{
					if (strcmp(extension_name, properties.extensionName) == 0)
					{
						l->debug(std::format("Requiring instance extension: {}", extension_name));
						std::erase(required_instance_extensions, extension_name);
					}
				}
			}
			if (required_instance_extensions.size() != 0) return VK_ERROR_EXTENSION_NOT_PRESENT;
			return result;
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

			const VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
			if (result != VK_SUCCESS) return result;
			l->debug("Vulkan instance created successfully!");
			volkLoadInstance(instance);

			// Set the debug callback logger to use the graphics device logger.
			debug_callback_logger = l;
			graphics_created_bitmask |= graphics_created_bit_instance;
			return result;
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
			const VkResult result = vkCreateDebugUtilsMessengerEXT(instance, &create_debug_callback_info_ext, nullptr, &debug_messenger);
			if (result != VK_SUCCESS) return result;
			graphics_created_bitmask |= graphics_created_bit_debug_messenger;
			return result;
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
			VkResult result = vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);

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
			return result;
		}

		[[nodiscard]] VkResult query_device_layers() const
		{
			l->info("Querying device layers");
			uint32_t p_property_count = 0;
			VkResult result = vkEnumerateDeviceLayerProperties(physical_device, &p_property_count, nullptr);
			if (result != VK_SUCCESS) return result;
			l->debug(std::format("Found {} device layers", p_property_count));
			if (p_property_count == 0) return result;
			std::vector<VkLayerProperties> layers;
			layers.resize(p_property_count);
			result = vkEnumerateDeviceLayerProperties(physical_device, &p_property_count, layers.data());
			if (result != VK_SUCCESS) return result;
			for (VkLayerProperties lp : layers)
			{
				l->debug(std::format("Device layer name: {} layer description: {}", lp.layerName, lp.description));
			}
			return result;
		}

		VkResult query_device_extensions()
		{
			l->info("Querying device extensions");

			uint32_t p_property_count = 0;

			VkResult result = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &p_property_count, nullptr);
			if (result != VK_SUCCESS) return result;

			l->debug(std::format("Found {} device extensions", p_property_count));
			if (p_property_count == 0) return result;

			std::vector<VkExtensionProperties> extensions;
			extensions.resize(p_property_count);

			result = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &p_property_count, extensions.data());
			if (result != VK_SUCCESS) return result;

			// validate required device extensions
			std::vector<const char*> required_device_extensions(std::begin(default_device_extensions), std::end(default_device_extensions));

			// ReSharper disable once CppUseStructuredBinding
			for (VkExtensionProperties properties : extensions)
			{
				l->debug(std::format("Device extension name: {}", properties.extensionName));
				for (const char* extension_name : default_device_extensions)
				{
					if (strcmp(extension_name, properties.extensionName) == 0)
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
			return result;
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

			VkResult result = vkCreateDevice(physical_device, &device_create_info, nullptr, &device);
			if (result != VK_SUCCESS) return result;

			l->debug("Vulkan device created successfully!");
			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_INSTANCE;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(instance);
				debug_name.pObjectName = "rosy instance";
				if (result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
			}
			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_DEVICE;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(device);
				debug_name.pObjectName = "rosy device";
				if (result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
			}
			graphics_created_bitmask |= graphics_created_bit_device;
			return result;
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
				if (const VkResult result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
			}
			return VK_SUCCESS;
		}

		VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) const
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
				if (swapchain_details.capabilities.maxImageCount > 0 && swapchain_image_count > swapchain_details.capabilities.
					maxImageCount)
				{
					swapchain_image_count = static_cast<uint8_t>(swapchain_details.capabilities.maxImageCount);
				}
				frame_datas.resize(std::min(max_frames_in_flight, swapchain_image_count));
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

				if (const VkResult result = vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain); result != VK_SUCCESS) return result;
				graphics_created_bitmask |= graphics_created_bit_swapchain;
				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(swapchain);
					debug_name.pObjectName = "rosy swapchain";
					if (const VkResult result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
				}
			}

			{
				swapchain_extent = extent;

				swapchain_images.clear();
				auto count = static_cast<uint32_t>(swapchain_image_count);
				if (const auto result = vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr); result != VK_SUCCESS) return result;
				swapchain_images.resize(swapchain_image_count);
				if (const auto result = vkGetSwapchainImagesKHR(device, swapchain, &count, swapchain_images.data()); result != VK_SUCCESS) return result;

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
					if (const VkResult result = vkCreateImageView(device, &swap_chain_image_view_create_info, nullptr, &image_view); result != VK_SUCCESS) return result;
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

			VkResult result = VK_SUCCESS;
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

					if (result = vmaCreateImage(allocator, &draw_info, &r_img_alloc_info, &draw_image.image, &draw_image.allocation, nullptr); result != VK_SUCCESS) return result;
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

					result = vkCreateImageView(device, &draw_view_create_info, nullptr, &draw_image.image_view);
					if (result != VK_SUCCESS) return result;
					graphics_created_bitmask |= graphics_created_bit_draw_image_view;
				}

				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_IMAGE;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(draw_image.image);
					debug_name.pObjectName = "rosy draw image";
					if (result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
				}
				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(draw_image.image_view);
					debug_name.pObjectName = "rosy draw image view";
					if (result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
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

					if (result = vmaCreateImage(allocator, &depth_image_info, &r_img_alloc_info, &depth_image.image, &depth_image.allocation, nullptr); result != VK_SUCCESS) return result;
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

					result = vkCreateImageView(device, &depth_view_create_info, nullptr, &depth_image.image_view);
					if (result != VK_SUCCESS) return result;
					graphics_created_bitmask |= graphics_created_bit_depth_image_view;
				}
				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_IMAGE;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(depth_image.image);
					debug_name.pObjectName = "rosy depth image";
					if (result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
				}
				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(depth_image.image_view);
					debug_name.pObjectName = "rosy depth image view";
					if (result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
				}
			}

			return result;
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

			VkDescriptorPoolCreateInfo pool_create_info{};
			pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
			pool_create_info.maxSets = 1;
			pool_create_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
			pool_create_info.pPoolSizes = pool_sizes.data();

			VkResult result = vkCreateDescriptorPool(device, &pool_create_info, nullptr, &descriptor_pool);
			if (result != VK_SUCCESS) return result;
			graphics_created_bitmask |= graphics_created_bit_descriptor_set;

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

			VkDescriptorSetLayout set_layout{};
			result = vkCreateDescriptorSetLayout(device, &layout_create_info, nullptr, &set_layout);
			if (result != VK_SUCCESS) return result;
			descriptor_set_layout = set_layout;

			VkDescriptorSetAllocateInfo set_create_info{};
			set_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			set_create_info.descriptorPool = descriptor_pool;
			set_create_info.descriptorSetCount = 1;
			set_create_info.pSetLayouts = &set_layout;

			result = vkAllocateDescriptorSets(device, &set_create_info, &descriptor_set);
			if (result != VK_SUCCESS) return result;

			graphics_created_bitmask |= graphics_created_bit_descriptor_pool;
			return VK_SUCCESS;
		}

		VkResult init_command_pool()
		{
			l->info("Initializing command pool");
			VkCommandPoolCreateInfo pool_info{};
			pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			pool_info.queueFamilyIndex = queue_index;

			for (size_t i = 0; i < max_frames_in_flight; i++)
			{
				VkCommandPool command_pool{};
				if (const VkResult result = vkCreateCommandPool(device, &pool_info, nullptr, &command_pool); result !=
					VK_SUCCESS)
					return result;
				frame_datas[i].command_pool = command_pool;
				frame_datas[i].frame_graphics_created_bitmask |= graphics_created_bit_command_pool;
				{
					const auto obj_name = std::format("rosy command_pool {}", i);
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_COMMAND_POOL;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(command_pool);
					debug_name.pObjectName = obj_name.c_str();
					if (const auto result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
				}
			}
			return VK_SUCCESS;
		}

		[[nodiscard]] VkResult init_command_buffers()
		{
			l->info("Initializing command buffer");

			for (size_t i = 0; i < max_frames_in_flight; i++)
			{
				{
					// render command buffer
					VkCommandBufferAllocateInfo alloc_info{};
					alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
					alloc_info.commandPool = frame_datas[i].command_pool;
					alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
					alloc_info.commandBufferCount = 1;

					VkCommandBuffer command_buffer{};
					if (const VkResult result = vkAllocateCommandBuffers(device, &alloc_info, &command_buffer); result != VK_SUCCESS) return result;
					frame_datas[i].command_buffer = command_buffer;
					{
						const auto obj_name = std::format("rosy command_buffer {}", i);
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(command_buffer);
						debug_name.pObjectName = obj_name.c_str();
						if (const auto result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
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

			VkResult result;
			for (size_t i = 0; i < max_frames_in_flight; i++)
			{
				{
					VkSemaphore semaphore;
					result = vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphore);
					if (result != VK_SUCCESS) return result;
					frame_datas[i].image_available_semaphore = semaphore;
					frame_datas[i].frame_graphics_created_bitmask |= graphics_created_bit_image_semaphore;
					{
						const auto obj_name = std::format("rosy image_available_semaphore {}", i);
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_SEMAPHORE;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(semaphore);
						debug_name.pObjectName = obj_name.c_str();
						if (result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
					}
				}
				{
					VkSemaphore semaphore;
					result = vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphore);
					if (result != VK_SUCCESS) return result;
					frame_datas[i].render_finished_semaphore = semaphore;
					frame_datas[i].frame_graphics_created_bitmask |= graphics_created_bit_pass_semaphore;
					{
						const auto obj_name = std::format("rosy render_finished_semaphore {}", i);
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_SEMAPHORE;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(semaphore);
						debug_name.pObjectName = obj_name.c_str();
						if (result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
					}
				}
				{
					VkFence fence;
					result = vkCreateFence(device, &fence_info, nullptr, &fence);
					if (result != VK_SUCCESS) return result;
					frame_datas[i].in_flight_fence = fence;
					frame_datas[i].frame_graphics_created_bitmask |= graphics_created_bit_fence;
					{
						const auto obj_name = std::format("rosy in_flight_fence {}", i);
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_FENCE;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(fence);
						debug_name.pObjectName = obj_name.c_str();
						if (result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
					}
				}
			}
			{
				result = vkCreateFence(device, &fence_info, nullptr, &immediate_fence);
				if (result != VK_SUCCESS) return result;
				graphics_created_bitmask |= graphics_created_bit_fence;
				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_FENCE;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(immediate_fence);
					debug_name.pObjectName = "rosy immediate_fence";
					if (result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
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

			if (const VkResult result = vkCreateDescriptorPool(device, &pool_info, nullptr, &ui_pool); result != VK_SUCCESS) return result;
			graphics_created_bitmask |= graphics_created_bit_ui_pool;

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
			init_info.MinImageCount = 2;
			init_info.ImageCount = 2;
			init_info.UseDynamicRendering = true;

			init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
			init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
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
			graphics_created_bitmask |= graphics_created_bit_depth_image;
			return VK_SUCCESS;
		}

		VkResult init_commands()
		{
			l->info("Initializing commands");

			VkCommandPoolCreateInfo pool_info{};
			pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			pool_info.queueFamilyIndex = queue_index;

			VkResult result = vkCreateCommandPool(device, &pool_info, nullptr, &immediate_command_pool);
			graphics_created_bitmask |= graphics_created_bit_command_pool;
			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_COMMAND_POOL;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(immediate_command_pool);
				debug_name.pObjectName = "rosy immediate command pool";
				if (result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
			}

			// allocate the command data for immediate submits
			VkCommandBufferAllocateInfo alloc_info{};
			alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			alloc_info.commandPool = immediate_command_pool;
			alloc_info.commandBufferCount = 1;
			alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

			result = vkAllocateCommandBuffers(device, &alloc_info, &immediate_command_buffer);
			if (result != VK_SUCCESS) return result;
			{
				VkDebugUtilsObjectNameInfoEXT debug_name{};
				debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				debug_name.pNext = nullptr;
				debug_name.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
				debug_name.objectHandle = reinterpret_cast<uint64_t>(immediate_command_buffer);
				debug_name.pObjectName = "rosy immediate command buffer";
				if (result = vkSetDebugUtilsObjectNameEXT(device, &debug_name); result != VK_SUCCESS) return result;
			}

			return VK_SUCCESS;
		}

		VkResult init_data()
		{
			l->info("Initializing data");
			return VK_SUCCESS;
		}

		VkResult init_ktx()
		{
			l->info("Initializing ktx");
			graphics_created_bitmask |= graphics_created_bit_ktx;
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
			// *** SETTING VERTEX BUFFER *** //
			const size_t vertex_buffer_size = a.positions.size() * sizeof(rosy_packager::position);
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
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_BUFFER;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(gpu_mesh.vertex_buffer.buffer);
					debug_name.pObjectName = "rosy vertex buffer";
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
			const size_t index_buffer_size = a.triangles.size() * sizeof(rosy_packager::triangle);
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
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_BUFFER;
					debug_name.objectHandle =  reinterpret_cast<uint64_t>(gpu_mesh.index_buffer.buffer);
					debug_name.pObjectName = "rosy index buffer";
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
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_BUFFER;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(staging.buffer);
					debug_name.pObjectName = "rosy staging";
					if (const VkResult res = vkSetDebugUtilsObjectNameEXT(device, &debug_name); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating staging buffer name: {}", static_cast<uint8_t>(res)));
						return result::error;
					}
				}
			}

			memcpy(staging.info.pMappedData, a.positions.data(), vertex_buffer_size);
			memcpy(static_cast<char*>(staging.info.pMappedData) + vertex_buffer_size, a.triangles.data(), index_buffer_size);


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

			{
				if (a.shaders.empty())
				{
					l->error("No shader loaded");
					return result::error;
				}
				std::vector<VkShaderCreateInfoEXT> shader_create_info;
				const auto [path, source] = a.shaders[0];

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

				gpu_mesh.shaders.resize(shader_create_info.size());

				if (const VkResult res = vkCreateShadersEXT(device, static_cast<uint32_t>(shader_create_info.size()), shader_create_info.data(), nullptr, gpu_mesh.shaders.data()); res != VK_SUCCESS) {
					l->error(std::format("Error creating shaders: {}", static_cast<uint8_t>(res)));
					return result::error;
				}
				gpu_mesh.graphics_created_bitmask |= graphics_created_bit_shaders;
				{
					VkDebugUtilsObjectNameInfoEXT debug_name{};
					debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
					debug_name.pNext = nullptr;
					debug_name.objectType = VK_OBJECT_TYPE_SHADER_EXT;
					debug_name.objectHandle = reinterpret_cast<uint64_t>(gpu_mesh.shaders.data()[0]);
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
					debug_name.objectHandle = reinterpret_cast<uint64_t>(gpu_mesh.shaders.data()[1]);
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
					if (const VkResult res = vkCreatePipelineLayout(device, &pl_info, nullptr, &gpu_mesh.layout); res != VK_SUCCESS)
					{
						l->error(std::format("Error creating shader pipeline layout: {}", static_cast<uint8_t>(res)));
						return result::error;
					}
					gpu_mesh.graphics_created_bitmask |= graphics_created_bit_pipeline_layout;
					{
						VkDebugUtilsObjectNameInfoEXT debug_name{};
						debug_name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
						debug_name.pNext = nullptr;
						debug_name.objectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
						debug_name.objectHandle = reinterpret_cast<uint64_t>(gpu_mesh.layout);
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

		result render()
		{
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
					{
						VkRenderingAttachmentInfo color_attachment{};
						color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
						color_attachment.pNext = nullptr;
						color_attachment.imageView = draw_image.image_view;
						color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
						color_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
						color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
						color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

						const auto render_area = VkRect2D{ VkOffset2D{0, 0}, swapchain_extent };
						VkRenderingInfo render_info{};
						render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
						render_info.pNext = nullptr;
						render_info.renderArea = render_area;
						render_info.layerCount = 1;
						render_info.colorAttachmentCount = 1;
						render_info.pColorAttachments = &color_attachment;
						render_info.pDepthAttachment = nullptr;
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

							vkCmdSetFrontFaceEXT(cf.command_buffer, VK_FRONT_FACE_COUNTER_CLOCKWISE);
							vkCmdSetCullModeEXT(cf.command_buffer, false);
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
							vkCmdSetLogicOpEnableEXT(cf.command_buffer, VK_FALSE);
							vkCmdSetDepthBounds(cf.command_buffer, 0.0f, 1.0f);
							vkCmdSetAlphaToCoverageEnableEXT(cf.command_buffer, VK_FALSE);
						}
						{
							constexpr auto enable = VK_FALSE;
							vkCmdSetColorBlendEnableEXT(cf.command_buffer, 0, 1, &enable);
						}
					}
					ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cf.command_buffer);
					vkCmdEndRendering(cf.command_buffer);
				}
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
				}
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

			current_frame = (current_frame + 1) % max_frames_in_flight;

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

	l->info("Graphics init done");


	return result::ok;
}

result graphics::set_asset(const rosy_packager::asset& a)
{
	l->debug("Setting asset!");
	if (const auto res = gd->set_asset(a); res != result::ok)
	{
		return res;
	}
	return result::ok;
}

result graphics::render()
{
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
	}
	{
		ImGui::ShowDemoWindow();
		ImGui::Render();
	}
	return gd->render();
}

result graphics::resize()
{
	return gd->resize_swapchain();
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


#pragma once

#include "memory_buffer.hpp"
#include <vulkan/vulkan_structs.hpp>

namespace alex {

struct uniform_descriptorsets_info_t {
	vk::PhysicalDevice physical_device;
	vk::Device device;
	vk::DescriptorPool pool;
	vk::DescriptorSetLayout layout;
	std::uint32_t set_count;
};

struct uniform_descriptorsets_update_info_t {
	vk::Device device;
	std::size_t set_index;
	alex::memory_buffer_t* buffer;
	vk::DeviceSize buffer_offset;
	vk::DeviceSize buffer_size;
};

struct uniform_descriptorsets_t {
	void init(uniform_descriptorsets_info_t& info);
	void update(uniform_descriptorsets_update_info_t& info);
	vk::DescriptorSet get_set(std::size_t i);
	
	std::vector<vk::DescriptorSet> sets;
};

}

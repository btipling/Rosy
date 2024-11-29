#pragma once
#include <vector>

VkDebugUtilsMessengerCreateInfoEXT create_debug_callback_info();

std::vector<char> read_file(const std::string& filename);

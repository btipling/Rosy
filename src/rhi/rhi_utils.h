#pragma once
#include <vector>
#include <fstream>

VkDebugUtilsMessengerCreateInfoEXT createDebugCallbackInfo();

std::vector<char> readFile(const std::string& filename);
#include <vector>
#include <fstream>
#include <glm/glm.hpp>
#include "../Rosy.h"

VkDebugUtilsMessengerCreateInfoEXT createDebugCallbackInfo();

std::vector<char> read_file(const std::string& filename);
#include <vector>
#include <fstream>
#include <glm/glm.hpp>
#include "../Rosy.h"

VkDebugUtilsMessengerCreateInfoEXT createDebugCallbackInfo();

std::vector<char> readFile(const std::string& filename);
#include "../utils/utils.h"
#include <unordered_map>
#include <filesystem>
#include "../Rosy.h"
#include "../rhi/rhi_types.h"

class rhi;

std::vector<char> read_file(const std::string& filename);
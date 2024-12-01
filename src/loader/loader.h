#include "../utils/utils.h"
#include <unordered_map>
#include <filesystem>
#include "../Rosy.h"
#include "../rhi/rhi_types.h"

class rhi;

std::optional<std::vector<std::shared_ptr<mesh_asset>>> load_gltf_meshes(rhi* rhi, std::filesystem::path file_path);
std::vector<char> read_file(const std::string& filename);
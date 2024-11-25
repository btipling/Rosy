#include "../utils/utils.h"
#include <unordered_map>
#include <filesystem>
#include "../Rosy.h"

class Rhi;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> load_gltf_meshes(Rhi* rhi, std::filesystem::path file_path);
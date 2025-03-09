#include "pch.h"
#include "Asset.h"

using namespace rosy_packager;

// Rosy File Format:
// 1. Header
// 2. GLTF Size Layouts: std::array<size_t,2> 
// index 0 - num materials
// index 1 - num samplers
// index 2 - num scenes
// index 3 - num nodes
// index 4 - num images
// index 5 - num meshes
// 3. a std::vector<material> of material size given
// 4. a std::vector<sampler> of sampler size given
// 5. Per scene
// 5a. Scene size layout std::array<size_t, 1>
//       // index 0 - num nodes -> a std::vector<uint32_t> of node indices
// 6. Per Node
// 6a. Node size layout: std::array<size_t, 3>
//       // index 0 - num world_translate -> always 1 to represent a single std::array<float, 3>
//       // index 1 - num world_scale -> always 1 to represent a single float
//       // index 2 - num world_yaw -> always 1 to represent a single float
//       // index 3 - num transforms -> always 1 to represent a single std::array<float, 16>
//       // index 4 -> num mesh ids -> always 1 to represent a single uint32_t
//       // index 5 -> num child_nodes -> a std::vector<uint32_t> to represent child node indices
//       // index 6 - num characters -> a std::vector<char> to represent a node name
// 7. Per Image
// 7a.Image size layout: std::array<size_t, 2>
// index 0 - image type uint32_t, an uint32_t but effectively an enum.
//       // index 1 - num characters -> a std::vector<char> to represent an image name
// 8. Per mesh:
// 8.a Mesh size layout: std::array<size_t,4>
// index 0 - num positions -> a std::vector<position> of positions size given
// index 1 - num indices -> a std::vector<uint32_t> of indices size given
// index 2 - num surfaces -> a std::vector<surface> of surfaces size given

rosy::result asset::write(const rosy::log* l)
{
    // OPEN FILE FOR WRITING BINARY

    FILE* stream{nullptr};

    l->debug(std::format("current file path: {}", std::filesystem::current_path().string()));

    if (const errno_t err = fopen_s(&stream, asset_path.c_str(), "wb"); err != 0)
    {
        l->error(std::format("failed to open for writing {}, {}", asset_path, err));
        return rosy::result::open_failed;
    }

    // WRITE RSY FORMAT HEADER

    {
        constexpr size_t num_headers = 1;
        const file_header header{
            .magic = rosy_format,
            .version = current_version,
            .endianness = 1, // for std::endian::little
            .coordinate_system = asset_coordinate_system,
            .root_scene = root_scene,
        };
        size_t res = fwrite(&header, sizeof(header), num_headers, stream);
        if (res != num_headers)
        {
            l->error(std::format("failed to write {}/{} headers", res, num_headers));
            return rosy::result::write_failed;
        }
        l->debug(std::format("wrote {} headers", res));
        l->info(std::format(
            "coordinate system: (\n{:.2f},{:.2f},{:.2f},{:.2f},\n{:.2f},{:.2f},{:.2f},{:.2f},\n{:.2f},{:.2f},{:.2f},{:.2f},\n{:.2f},{:.2f},{:.2f},{:.2f},\n)",
            asset_coordinate_system[0], asset_coordinate_system[1], asset_coordinate_system[2], asset_coordinate_system[3],
            asset_coordinate_system[4], asset_coordinate_system[5], asset_coordinate_system[6], asset_coordinate_system[7],
            asset_coordinate_system[8], asset_coordinate_system[9], asset_coordinate_system[10], asset_coordinate_system[11],
            asset_coordinate_system[12], asset_coordinate_system[13], asset_coordinate_system[14], asset_coordinate_system[15]
        ));
    }

    // WRITE GLTF SIZES FOR ASSET RESOURCES

    {
        const size_t num_materials{materials.size()};
        const size_t num_samplers{samplers.size()};
        const size_t num_scenes{scenes.size()};
        const size_t num_nodes{nodes.size()};
        const size_t num_images{images.size()};
        const size_t num_meshes{meshes.size()};

        constexpr size_t lookup_sizes = 1;
        const std::array<size_t, 6> sizes{
            num_materials,
            num_samplers,
            num_scenes,
            num_nodes,
            num_images,
            num_meshes,
        };
        size_t res = fwrite(&sizes, sizeof(sizes), lookup_sizes, stream);
        if (res != lookup_sizes)
        {
            l->error(std::format("failed to write {}/{} num_gltf_sizes", res, lookup_sizes));
            return rosy::result::write_failed;
        }
        l->debug(std::format(
            "wrote {} sizes, num_materials: {}, num_samplers: {}, num_scenes: {}, num_nodes: {}, num_images: {}, num_meshes: {}",
            res,
            num_materials,
            num_samplers,
            num_scenes,
            num_nodes,
            num_images,
            num_meshes));
    }

    // WRITE MATERIALS

    {
        size_t res = fwrite(materials.data(), sizeof(material), materials.size(), stream);
        if (res != materials.size())
        {
            l->error(std::format("failed to write {}/{} materials", res, materials.size()));
            return rosy::result::write_failed;
        }
        l->debug(std::format("wrote {} materials", res));
    }

    // WRITE SAMPLERS

    {
        size_t res = fwrite(samplers.data(), sizeof(sampler), samplers.size(), stream);
        if (res != samplers.size())
        {
            l->error(std::format("failed to write {}/{} samplers", res, samplers.size()));
            return rosy::result::write_failed;
        }
        l->debug(std::format("wrote {} samplers", res));
    }

    // WRITE ALL THE SCENES ONE AT A TIME

    for (const auto& [scene_nodes] : scenes)
    {
        // WRITE ONE SCENE SIZE

        {
            constexpr size_t lookup_sizes = 1;
            const size_t num_scene_nodes{scene_nodes.size()};
            const std::array<size_t, 1> scene_sizes{num_scene_nodes};
            size_t res = fwrite(&scene_sizes, sizeof(scene_sizes), lookup_sizes, stream);
            if (res != lookup_sizes)
            {
                l->error(std::format("failed to write {}/{} scene_sizes", res, lookup_sizes));
                return rosy::result::write_failed;
            }
            l->debug(std::format(
                "wrote {} sizes, num_nodes: {}",
                res, num_scene_nodes));
        }

        // WRITE ONE SCENE NODES

        {
            size_t res = fwrite(scene_nodes.data(), sizeof(uint32_t), scene_nodes.size(), stream);
            if (res != scene_nodes.size())
            {
                l->error(std::format("failed to write {}/{} scene nodes", res, scene_nodes.size()));
                return rosy::result::write_failed;
            }
            l->debug(std::format("wrote {} scene nodes", res));
        }
    }

    // WRITE ALL NODES ONE AT A TIME

    for (const auto& [
             world_translate,
             world_scale,
             world_yaw,
             coordinate_transform,
             is_world_node,
             transform,
             mesh_id,
             child_nodes,
             node_name_chars] : nodes)
    {
        // WRITE ONE NODE SIZE

        constexpr size_t num_world_translate{1};
        constexpr size_t num_world_scale{1};
        constexpr size_t num_world_yaw{1};
        constexpr size_t num_transforms{1};
        constexpr size_t num_mesh_ids{1};

        {
            const size_t num_child_nodes{child_nodes.size()};
            const size_t num_name_chars{node_name_chars.size()};
            constexpr size_t lookup_sizes = 1;
            const std::array<size_t, 7> node_sizes{num_world_translate, num_world_scale, num_world_yaw, num_transforms, num_mesh_ids, num_child_nodes, num_name_chars};
            size_t res = fwrite(&node_sizes, sizeof(node_sizes), lookup_sizes, stream);
            if (res != lookup_sizes)
            {
                l->error(std::format("failed to write {}/{} node_sizes", res, lookup_sizes));
                return rosy::result::write_failed;
            }
            l->debug(std::format(
                "wrote {} sizes, num_world_translate: {}, num_world_scale: {}, num_world_yaw: {}, num_transforms: {} num_mesh_ids: {}, num_child_nodes: {},  num_name_chars: {}",
                res, num_world_translate, num_world_scale, num_world_yaw, num_transforms, num_mesh_ids, num_child_nodes, num_name_chars));
        }

        // WRITE ONE NODE world_translate

        {
            size_t res = fwrite(&world_translate, sizeof(world_translate), num_world_translate, stream);
            if (res != num_world_translate)
            {
                l->error(std::format("failed to write {}/{} node world_translate", res, num_world_translate));
                return rosy::result::write_failed;
            }
            l->debug(std::format("wrote {} node world_translate", res));
        }

        // WRITE ONE NODE world_scale

        {
            size_t res = fwrite(&world_scale, sizeof(world_scale), num_world_scale, stream);
            if (res != num_world_scale)
            {
                l->error(std::format("failed to write {}/{} node world_scale", res, num_world_scale));
                return rosy::result::write_failed;
            }
            l->debug(std::format("wrote {} node world_scale", res));
        }

        // WRITE ONE NODE CUSTOM_world_yaw

        {
            size_t res = fwrite(&world_yaw, sizeof(world_yaw), num_world_yaw, stream);
            if (res != num_world_yaw)
            {
                l->error(std::format("failed to write {}/{} node world_yaw", res, num_world_yaw));
                return rosy::result::write_failed;
            }
            l->debug(std::format("wrote {} node world_yaw", res));
        }

        // WRITE ONE NODE TRANSFORM

        {
            size_t res = fwrite(&transform, sizeof(transform), num_transforms, stream);
            if (res != num_transforms)
            {
                l->error(std::format("failed to write {}/{} node transforms", res, num_transforms));
                return rosy::result::write_failed;
            }
            l->debug(std::format("wrote {} node transforms", res));
        }

        // WRITE ONE NODE MESH INDEX

        {
            size_t res = fwrite(&mesh_id, sizeof(mesh_id), num_mesh_ids, stream);
            if (res != num_mesh_ids)
            {
                l->error(std::format("failed to write {}/{} node mesh ids", res, num_mesh_ids));
                return rosy::result::write_failed;
            }
            l->debug(std::format("wrote {} node mesh ids", res));
        }

        // WRITE ONE NODE CHILD NODES

        {
            size_t res = fwrite(child_nodes.data(), sizeof(uint32_t), child_nodes.size(), stream);
            if (res != child_nodes.size())
            {
                l->error(std::format("failed to write {}/{} node child nodes", res, child_nodes.size()));
                return rosy::result::write_failed;
            }
            l->debug(std::format("wrote {} node child nodes", res));
        }

        // WRITE ONE NODE NAME CHARS

        {
            size_t res = fwrite(node_name_chars.data(), sizeof(char), node_name_chars.size(), stream);
            if (res != node_name_chars.size())
            {
                l->error(std::format("failed to write {}/{} node name chars", res, node_name_chars.size()));
                return rosy::result::write_failed;
            }
            l->debug(std::format("wrote {} node node name chars", res));
        }
    }

    // WRITE ALL IMAGES ONE AT A TIME

    for (const auto& [image_type, names] : images)
    {
        // WRITE ONE IMAGE SIZE

        constexpr size_t num_image_types{1};
        {
            const size_t num_chars{names.size()};
            constexpr size_t lookup_sizes = 1;
            const std::array<size_t, 2> image_sizes{num_image_types, num_chars};
            size_t res = fwrite(&image_sizes, sizeof(image_sizes), lookup_sizes, stream);
            if (res != lookup_sizes)
            {
                l->error(std::format("failed to write {}/{} image_sizes", res, lookup_sizes));
                return rosy::result::write_failed;
            }
            l->debug(std::format(
                "wrote {} sizes, num_image_types: {} num image name chars: {}",
                res, num_image_types, num_chars));
        }

        // WRITE ONE IMAGE TYPE
        {
            size_t res = fwrite(&image_type, sizeof(uint32_t), num_image_types, stream);
            if (res != num_image_types)
            {
                l->error(std::format("failed to write {}/{} image image_type", res, num_image_types));
                return rosy::result::write_failed;
            }
            l->debug(std::format("wrote {} image image_type", res));
        }

        // WRITE ONE IMAGE NAME

        {
            size_t res = fwrite(names.data(), sizeof(char), names.size(), stream);
            if (res != names.size())
            {
                l->error(std::format("failed to write {}/{} image name chars", res, names.size()));
                return rosy::result::write_failed;
            }
            l->debug(std::format("wrote {} image name chars", res));
        }
    }

    // WRITE ALL MESHES ONE AT A TIME

    for (const auto& [positions, indices, surfaces] : meshes)
    {
        // WRITE ONE MESH SIZE

        {
            const size_t num_positions{positions.size()};
            const size_t num_indices{indices.size()};
            const size_t num_surfaces{surfaces.size()};
            constexpr size_t lookup_sizes = 1;
            const std::array<size_t, 3> mesh_sizes{num_positions, num_indices, num_surfaces};
            size_t res = fwrite(&mesh_sizes, sizeof(mesh_sizes), lookup_sizes, stream);
            if (res != lookup_sizes)
            {
                l->error(std::format("failed to write {}/{} num_mesh_sizes", res, lookup_sizes));
                return rosy::result::write_failed;
            }
            l->debug(std::format(
                "wrote {} sizes, num_positions: {} num_indices: {} num_surfaces: {}",
                res, num_positions, num_indices, num_surfaces));
        }

        // WRITE ONE MESH POSITIONS

        {
            size_t res = fwrite(positions.data(), sizeof(position), positions.size(), stream);
            if (res != positions.size())
            {
                l->error(std::format("failed to write {}/{} positions", res, positions.size()));
                return rosy::result::write_failed;
            }
            l->info(std::format("wrote {} positions", res));
        }

        // WRITE ONE MESH INDICES

        {
            size_t res = fwrite(indices.data(), sizeof(uint32_t), indices.size(), stream);
            if (res != indices.size())
            {
                l->error(std::format("failed to write {}/{} indices", res, indices.size()));
                return rosy::result::write_failed;
            }
            l->debug(std::format("wrote {} indices", res));
        }

        // WRITE ONE MESH SURFACES

        {
            size_t res = fwrite(surfaces.data(), sizeof(surface), surfaces.size(), stream);
            if (res != surfaces.size())
            {
                l->error(std::format("failed to write {}/{} surfaces", res, surfaces.size()));
                return rosy::result::write_failed;
            }
            l->debug(std::format("wrote {} surfaces", res));
        }
    }

    int num_closed = fclose(stream);

    l->debug(std::format("closed {} files", num_closed));

    return rosy::result::ok;
}

rosy::result asset::read(rosy::log* l)
{
    // OPEN FILE FOR READING BINARY

    FILE* stream{nullptr};

    {
        l->debug(std::format("current file path: {}", std::filesystem::current_path().string()));
        if (const errno_t err = fopen_s(&stream, asset_path.c_str(), "rb"); err != 0)
        {
            l->error(std::format("failed to open for reading {}, {}", asset_path, err));
            return rosy::result::open_failed;
        }
    }

    // READ RSY FORMAT HEADER

    {
        constexpr size_t num_headers = 1;
        file_header header{
            .magic = 0,
            .version = 0,
            .endianness = 0,
            .coordinate_system = {},
            .root_scene = 0,
        };
        size_t res = fread(&header, sizeof(header), num_headers, stream);
        if (res != num_headers)
        {
            l->error(std::format("failed to read {}/{} headers", res, num_headers));
            return rosy::result::read_failed;
        }
        if (header.magic != rosy_format)
        {
            l->error(std::format("failed to read, magic mismatch, got: {} should be {}", header.magic, rosy_format));
            return rosy::result::read_failed;
        }
        if (header.version != current_version)
        {
            l->error(std::format("failed to read, version mismatch file is version {} current version is {}", header.version, current_version));
            return rosy::result::read_failed;
        }
        constexpr uint32_t is_little_endian = 1; // This always true: std::endian::native == std::endian::little 
        // NOLINT(clang-diagnostic-unreachable-code)
        if (header.endianness != is_little_endian)
        {
            l->error(std::format("failed to read, endianness mismatch file is {} system is {}", header.endianness, is_little_endian));
            return rosy::result::read_failed;
        }
        asset_coordinate_system = header.coordinate_system;
        root_scene = header.root_scene;
        l->debug(std::format("read {} headers", res));
        l->debug(std::format("format version: {} is little endian: {} root scene: {}", header.version, is_little_endian, root_scene));
        l->info(std::format(
            "coordinate system: (\n{:.2f},{:.2f},{:.2f},{:.2f},\n{:.2f},{:.2f},{:.2f},{:.2f},\n{:.2f},{:.2f},{:.2f},{:.2f},\n{:.2f},{:.2f},{:.2f},{:.2f},\n)",
            asset_coordinate_system[0], asset_coordinate_system[1], asset_coordinate_system[2], asset_coordinate_system[3],
            asset_coordinate_system[4], asset_coordinate_system[5], asset_coordinate_system[6], asset_coordinate_system[7],
            asset_coordinate_system[8], asset_coordinate_system[9], asset_coordinate_system[10], asset_coordinate_system[11],
            asset_coordinate_system[12], asset_coordinate_system[13], asset_coordinate_system[14], asset_coordinate_system[15]
        ));
    }

    // WRITE GLTF SIZES FOR ALL ASSET RESOURCES

    size_t num_materials{0};
    size_t num_samplers{0};
    size_t num_scenes{0};
    size_t num_nodes{0};
    size_t num_images{0};
    size_t num_meshes{0};
    {
        constexpr size_t lookup_sizes = 1;
        std::array<size_t, 6> num_gltf_sizes{0, 0, 0, 0, 0, 0};
        size_t res = fread(&num_gltf_sizes, sizeof(num_gltf_sizes), lookup_sizes, stream);
        if (res != lookup_sizes)
        {
            l->error(std::format("failed to read {}/{} num_gltf_sizes", res, lookup_sizes));
            return rosy::result::read_failed;
        }
        l->debug(std::format("read {} sizes", res));

        num_materials = num_gltf_sizes[0];
        num_samplers = num_gltf_sizes[1];
        num_scenes = num_gltf_sizes[2];
        num_nodes = num_gltf_sizes[3];
        num_images = num_gltf_sizes[4];
        num_meshes = num_gltf_sizes[5];
    }

    materials.resize(num_materials);
    samplers.resize(num_samplers);
    scenes.reserve(num_scenes);
    nodes.reserve(num_nodes);
    meshes.reserve(num_meshes);

    // READ GLTF MATERIALS

    {
        size_t res = fread(materials.data(), sizeof(material), num_materials, stream);
        if (res != num_materials)
        {
            l->error(std::format("failed to read {}/{} materials", res, num_materials));
            return rosy::result::read_failed;
        }
        l->debug(std::format("read {} materials", res));
    }

    // READ GLTF SAMPLERS

    {
        size_t res = fread(samplers.data(), sizeof(sampler), num_samplers, stream);
        if (res != num_samplers)
        {
            l->error(std::format("failed to read {}/{} samplers", res, num_samplers));
            return rosy::result::read_failed;
        }
        l->debug(std::format("read {} samplers", res));
    }

    // READ ALL THE SCENES ONE AT A TIME

    for (size_t i{0}; i < num_scenes; i++)
    {
        scene s{};

        // READ ONE SCENE SIZE

        size_t num_scene_nodes{0};
        {
            constexpr size_t lookup_sizes = 1;
            std::array<size_t, 1> scene_sizes{};
            size_t res = fread(&scene_sizes, sizeof(scene_sizes), lookup_sizes, stream);
            if (res != lookup_sizes)
            {
                l->error(std::format("failed to read {}/{} scene_sizes", res, lookup_sizes));
                return rosy::result::read_failed;
            }
            num_scene_nodes = scene_sizes[0];
            l->debug(std::format(
                "read {} sizes, num_nodes: {} for scene",
                res, num_scene_nodes));
        }

        s.nodes.resize(num_scene_nodes);

        // READ ONE SCENE NODES

        {
            size_t res = fread(s.nodes.data(), sizeof(uint32_t), num_scene_nodes, stream);
            if (res != num_scene_nodes)
            {
                l->error(std::format("failed to read {}/{} scene nodes", res, num_scene_nodes));
                return rosy::result::read_failed;
            }
            l->debug(std::format("read {} scene nodes", res));
        }
        scenes.push_back(s);
    }

    // READ ALL NODES ONE AT A TIME

    for (size_t i{0}; i < num_nodes; i++)
    {
        node n{};

        // READ ONE NODE SIZE

        size_t num_world_translate{0};
        size_t num_world_scale{0};
        size_t num_world_yaw{0};
        size_t num_transforms{0};
        size_t num_mesh_ids{0};
        size_t num_child_nodes{0};
        size_t num_node_name_chars{0};
        {
            constexpr size_t lookup_sizes = 1;
            std::array<size_t, 7> node_sizes{0, 0, 0, 0, 0, 0, 0};
            size_t res = fread(&node_sizes, sizeof(node_sizes), lookup_sizes, stream);
            if (res != lookup_sizes)
            {
                l->error(std::format("failed to read {}/{} node_sizes", res, lookup_sizes));
                return rosy::result::read_failed;
            }
            num_world_translate = node_sizes[0];
            num_world_scale = node_sizes[1];
            num_world_yaw = node_sizes[2];
            num_transforms = node_sizes[3];
            num_mesh_ids = node_sizes[4];
            num_child_nodes = node_sizes[5];
            num_node_name_chars = node_sizes[6];
            l->debug(std::format(
                "read {} sizes, num_transforms: {} num_mesh_ids: {} num_child_nodes: {} num_node_name_chars: {}",
                res, num_transforms, num_mesh_ids, num_child_nodes, num_node_name_chars));
            assert(num_world_translate == 1);
            assert(num_world_scale == 1);
            assert(num_world_yaw == 1);
            assert(num_transforms == 1);
            assert(num_mesh_ids == 1);
        }

        n.child_nodes.resize(num_child_nodes);
        n.name.resize(num_node_name_chars);

        // READ ONE NODE world_translate

        {
            size_t res = fread(&n.world_translate, sizeof(std::array<float, 3>), num_world_translate, stream);
            if (res != num_world_translate)
            {
                l->error(std::format("failed to read {}/{} node world_translate", res, num_world_translate));
                return rosy::result::read_failed;
            }
            l->debug(std::format("read {} node world_translate", res));
        }

        // READ ONE NODE world_scale

        {
            size_t res = fread(&n.world_scale, sizeof(float), num_world_scale, stream);
            if (res != num_world_scale)
            {
                l->error(std::format("failed to read {}/{} node world_scale", res, num_world_scale));
                return rosy::result::read_failed;
            }
            l->debug(std::format("read {} node world_scale", res));
        }

        // READ ONE NODE world_yaw

        {
            size_t res = fread(&n.world_yaw, sizeof(float), num_world_yaw, stream);
            if (res != num_world_yaw)
            {
                l->error(std::format("failed to read {}/{} node world_yaw", res, num_world_yaw));
                return rosy::result::read_failed;
            }
            l->debug(std::format("read {} node world_yaw", res));
        }

        // READ ONE NODE TRANSFORM

        {
            size_t res = fread(&n.transform, sizeof(std::array<float, 16>), num_transforms, stream);
            if (res != num_transforms)
            {
                l->error(std::format("failed to read {}/{} node transform", res, num_transforms));
                return rosy::result::read_failed;
            }
            l->debug(std::format("read {} node transform", res));
        }

        // READ ONE NODE MESH ID

        {
            size_t res = fread(&n.mesh_id, sizeof(uint32_t), num_mesh_ids, stream);
            if (res != num_mesh_ids)
            {
                l->error(std::format("failed to read {}/{} node mesh ids", res, num_mesh_ids));
                return rosy::result::read_failed;
            }
            l->debug(std::format("read {} node mesh ids", res));
        }

        // READ ONE NODE CHILD NODES

        {
            size_t res = fread(n.child_nodes.data(), sizeof(uint32_t), num_child_nodes, stream);
            if (res != num_child_nodes)
            {
                l->error(std::format("failed to read {}/{} node child nodes", res, num_child_nodes));
                return rosy::result::read_failed;
            }
            l->debug(std::format("read {} node child nodes", res));
        }

        // READ ONE NODE NAME CHARS

        {
            size_t res = fread(n.name.data(), sizeof(char), num_node_name_chars, stream);
            if (res != num_node_name_chars)
            {
                l->error(std::format("failed to read {}/{} node name chars", res, num_node_name_chars));
                return rosy::result::read_failed;
            }
            l->debug(std::format("read {} node name chars", res));
        }

        // ADD MESH TO ASSET

        nodes.push_back(n);
    }

    // READ ALL IMAGES ONE AT A TIME

    for (size_t i{0}; i < num_images; i++)
    {
        image img{};

        // READ ONE IMAGE SIZE

        size_t num_image_types{0};
        size_t num_chars{0};
        {
            constexpr size_t lookup_sizes = 1;
            std::array<size_t, 2> image_sizes{0, 0};
            size_t res = fread(&image_sizes, sizeof(image_sizes), lookup_sizes, stream);
            if (res != lookup_sizes)
            {
                l->error(std::format("failed to read {}/{} image_sizes", res, lookup_sizes));
                return rosy::result::read_failed;
            }
            num_image_types = image_sizes[0];
            num_chars = image_sizes[1];
            l->debug(std::format(
                "read {} sizes, num_image_types: {}, num_chars: {}",
                res, num_image_types, num_chars));
        }
        assert(num_image_types == 1);

        img.name.resize(num_chars);

        // READ ONE IMAGE TYPE

        {
            size_t res = fread(&img.image_type, sizeof(uint32_t), num_image_types, stream);
            if (res != num_image_types)
            {
                l->error(std::format("failed to read {}/{} image image_type", res, num_image_types));
                return rosy::result::read_failed;
            }
            l->debug(std::format("read {} image image_type", res));
        }

        // READ ONE IMAGE NAME

        {
            size_t res = fread(img.name.data(), sizeof(char), num_chars, stream);
            if (res != num_chars)
            {
                l->error(std::format("failed to read {}/{} image name chars", res, num_chars));
                return rosy::result::read_failed;
            }
            l->debug(std::format("read {} image name chars", res));
        }

        // ADD IMAGE TO ASSET

        images.push_back(img);
    }

    // READ ALL MESHES ONE AT A TIME

    for (size_t i{0}; i < num_meshes; i++)
    {
        mesh m{};

        // READ ONE MESH SIZE

        size_t num_positions{0};
        size_t num_indices{0};
        size_t num_surfaces{0};
        {
            constexpr size_t lookup_sizes = 1;
            std::array<size_t, 3> mesh_sizes{0, 0, 0};
            size_t res = fread(&mesh_sizes, sizeof(mesh_sizes), lookup_sizes, stream);
            if (res != lookup_sizes)
            {
                l->error(std::format("failed to read {}/{} num_mesh_sizes", res, lookup_sizes));
                return rosy::result::read_failed;
            }
            num_positions = mesh_sizes[0];
            num_indices = mesh_sizes[1];
            num_surfaces = mesh_sizes[2];
            l->debug(std::format(
                "read {} sizes, num_positions: {} num_indices: {} num_surfaces: {}",
                res, num_positions, num_indices, num_surfaces));
        }

        m.positions.resize(num_positions);
        m.indices.resize(num_indices);
        m.surfaces.resize(num_surfaces);

        // READ ONE MESH POSITIONS

        {
            size_t res = fread(m.positions.data(), sizeof(position), num_positions, stream);
            if (res != num_positions)
            {
                l->error(std::format("failed to read {}/{} positions", res, num_positions));
                return rosy::result::read_failed;
            }
            l->debug(std::format("read {} positions", res));
        }

        // READ ONE MESH INDICES

        {
            size_t res = fread(m.indices.data(), sizeof(uint32_t), num_indices, stream);
            if (res != num_indices)
            {
                l->error(std::format("failed to read {}/{} indices", res, num_indices));
                return rosy::result::read_failed;
            }
            l->debug(std::format("read {} indices", res));
        }

        // READ ONE MESH SURFACES

        {
            size_t res = fread(m.surfaces.data(), sizeof(surface), num_surfaces, stream);
            if (res != num_surfaces)
            {
                l->error(std::format("failed to read {}/{} surfaces", res, num_surfaces));
                return rosy::result::read_failed;
            }
            l->debug(std::format("read {} surfaces", res));
        }

        // ADD MESH TO ASSET

        meshes.push_back(m);
    }

    int num_closed = fclose(stream);

    l->debug(std::format("closed {} files", num_closed));
    return rosy::result::ok;
}

rosy::result asset::read_shaders(const std::shared_ptr<rosy::log>& l)
{
    // ReSharper disable once CppUseStructuredBinding
    for (shader& s : shaders)
    {
        std::ifstream file(s.path, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            l->error(std::format("failed to open shader {}", s.path));
            return rosy::result::open_failed;
        }

        const std::streamsize file_size = file.tellg();
        if (file_size < 1)
        {
            l->error(std::format("invalid shader source {}", s.path));
            return rosy::result::read_failed;
        }
        std::vector<char> buffer(file_size);
        s.source.resize(file_size);
        file.seekg(0);
        file.read(s.source.data(), file_size);
        file.close();
        if (static_cast<std::streamsize>(s.source.size()) != file_size)
        {
            l->error(std::format("failed to read shader source {}", s.path));
            return rosy::result::read_failed;
        }

        l->debug(std::format("shader file size: {}", file_size));
    }
    l->debug(std::format("read {} shaders", shaders.size()));
    return rosy::result::ok;
}

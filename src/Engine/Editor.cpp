#include "Editor.h"

#include <cassert>
#include <filesystem>
#include <queue>
#include <fstream>
#include <nlohmann/json.hpp>
#include "../Packager/Asset.h"

using json = nlohmann::json;
using namespace rosy;

namespace rosy_editor
{
    struct level_data_model
    {
        std::string id{};
        std::string name{};
        std::array<float, 3> location{};
        float scale{1.f};
        float yaw{0.f};
        uint32_t model_type{0};
    };

    struct level_data
    {
        std::vector<level_data_model> models;
    };

    void to_json(json& j, const level_data_model& model) // NOLINT(misc-use-internal-linkage)
    {
        j = json{
            {"id", model.id},
            {"name", model.name},
            {"location", model.location},
            {"scale", model.scale},
            {"yaw", model.yaw},
            {"model_type", model.model_type},
        };
    }

    void from_json(const json& j, level_data_model& p) // NOLINT(misc-use-internal-linkage)
    {
        j.at("id").get_to(p.id);
        j.at("name").get_to(p.name);
        j.at("location").get_to(p.location);
        if (j.contains("scale"))
        {
            j.at("scale").get_to(p.scale);
        }
        j.at("yaw").get_to(p.yaw);
        j.at("model_type").get_to(p.model_type);
    }

    void to_json(json& j, const level_data& l) // NOLINT(misc-use-internal-linkage)
    {
        j = json{{"models", l.models}};
    }

    void from_json(const json& j, level_data& l) // NOLINT(misc-use-internal-linkage)
    {
        j.at("models").get_to(l.models);
    }
}

namespace
{
    struct stack_item
    {
        std::string id{};
        rosy_packager::node stack_node;
    };

    struct level_asset_builder_index_map
    {
        uint32_t source_index{0};
        uint32_t destination_index{0};
    };

    struct level_asset_builder_source_asset_helper
    {
        std::string asset_id{};
        size_t rosy_package_asset_index{0};
        std::vector<level_asset_builder_index_map> sampler_mappings;
        std::vector<level_asset_builder_index_map> image_mappings;
        std::vector<level_asset_builder_index_map> material_mappings;
        std::vector<level_asset_builder_index_map> node_mappings;
        std::vector<level_asset_builder_index_map> mesh_mappings;
    };

    struct level_asset_builder
    {
        std::vector<level_asset_builder_source_asset_helper> assets;
    };

    struct editor_manager
    {
        rosy::log* l{nullptr};
        rosy_packager::asset level_asset;
        std::vector<rosy_packager::asset*> origin_assets;
        std::vector<asset_description> asset_descriptions;
        rosy_editor::level_data ld;

        result init(rosy::log* new_log)
        {
            l = new_log;
            return result::ok;
        }

        void deinit()
        {
            l = nullptr;
            for (asset_description& desc : asset_descriptions)
            {
                const auto a = static_cast<const rosy_packager::asset*>(desc.asset);
                delete a;
                desc.asset = nullptr;
            }
        }

        [[nodiscard]] result add_model(std::string id, editor_command::model_type type)
        {
            for (const auto& a : asset_descriptions)
            {
                if (a.id.size() >= id.size()) continue;
                if (std::equal(a.id.begin(), a.id.end(), id.begin(), id.begin() + static_cast<uint64_t>(a.id.size())))
                {
                    l->info(std::format("add_model: asset id {} is a prefix of id: {}", a.id, id));
                    for (const auto& asset_model : a.models)
                    {
                        if (asset_model.id == id)
                        {
                            rosy_editor::level_data_model md;
                            md.id = asset_model.id;
                            md.name = asset_model.name;
                            md.location = asset_model.location;
                            md.scale = asset_model.scale;
                            md.yaw = asset_model.yaw;
                            md.model_type = static_cast<uint8_t>(type);
                            ld.models.push_back(md);
                            return result::ok;
                        }
                        l->info(std::format("add_model: model id {} does not match {}", asset_model.id, id));
                    }
                }
                else
                {
                    l->info(std::format("add_model: asset id {} is not a prefix of id: {}", a.id, id));
                }
            }
            l->info(std::format("add_model: id: {} not found", id));
            return result::ok;
        }

        [[nodiscard]] result edit_node(const std::string& id, const editor_command::model_type model_type, const editor_command_node_data& node_data)
        {
            switch (model_type)
            {
            case editor_command::model_type::no_model:
                return result::error; // This can't happen.
            case editor_command::model_type::mob_model:
            case editor_command::model_type::static_model:
                size_t index{0};
                bool found{false};
                for (const auto& md : ld.models)
                {
                    if (static_cast<editor_command::model_type>(md.model_type) == model_type && md.id == id)
                    {
                        found = {true};
                        break;
                    }
                    index += 1;
                }
                if (!found)
                {
                    l->warn(std::format("Attempted to a model that wasn't found: {}", id));
                    return result::ok;
                }
                ld.models[index].location = node_data.location;
                ld.models[index].scale = node_data.scale;
                ld.models[index].yaw = node_data.yaw;
            }
            return result::ok;
        }

        [[nodiscard]] result remove_model(const std::string& id)
        {
            bool found{false};
            size_t index{0};
            for (const auto& md : ld.models)
            {
                if (md.id == id)
                {
                    found = true;
                    break;
                }
                index += 1;
            }
            if (!found) return result::ok;
            ld.models.erase(ld.models.begin() + static_cast<uint64_t>(index));
            return result::ok;
        }

        [[nodiscard]] result write() const
        {
            try
            {
                std::ofstream o("level1.json");
                json j;
                to_json(j, ld);
                o << std::setw(4) << j << '\n';
            }
            catch (std::exception& e)
            {
                l->error(std::format("error writing level {}", e.what()));
                return result::error;
            }
            catch (...)
            {
                l->error("error unknown exception writing level");
                return result::error;
            }
            return result::ok;
        }

        [[nodiscard]] result read()
        {
            try
            {
                std::ifstream i("level1.json");
                json j;
                i >> j;

                ld = {};
                from_json(j, ld);
            }
            catch (std::exception& e)
            {
                l->error(std::format("error reading level {}", e.what()));
                return result::error;
            }
            catch (...)
            {
                l->error("error unknown exception reading level");
                return result::error;
            }

            for (const auto& m : ld.models)
            {
                l->info(std::format("id: {} name: {} location: ({:.3f}, {:.3f}, {:.3f}) scale: {:.3f} yaw: {:.3f}", m.id, m.name, m.location[0], m.location[1], m.location[2],
                                    m.scale, m.yaw));
            }
            return result::ok;
        }

        [[nodiscard]] result process(const level_editor_commands& commands, level_editor_state* state)
        {
            state->new_asset = nullptr;
            if (!asset_descriptions.empty())
            {
                for (const auto& cmd : commands.commands)
                {
                    switch (cmd.command_type)
                    {
                    case editor_command::editor_command_type::no_command:
                        l->info("editor-command: no_command command detected.");
                        break;
                    case editor_command::editor_command_type::load_asset:
                        l->info(std::format("editor-command: load_asset command detected for id: {}", cmd.id));
                        for (const auto& a : asset_descriptions)
                        {
                            if (a.id == cmd.id)
                            {
                                state->new_asset = a.asset;
                                return result::ok;
                            }
                        }
                        l->warn(std::format("editor-command: attempted to load an unknown asset {}", cmd.id));
                        break;
                    case editor_command::editor_command_type::write_level:
                        l->info("editor-command: saving level.");
                        if (const auto res = write(); res != result::ok)
                        {
                            l->error(std::format("error writing level file {}", static_cast<uint8_t>(res)));
                            return res;
                        }
                        break;
                    case editor_command::editor_command_type::read_level:
                        l->info("editor-command: loading level.");
                        if (const auto res = read(); res != result::ok)
                        {
                            l->error(std::format("error writing level file {}", static_cast<uint8_t>(res)));
                            return res;
                        }
                        if (const result res = load_level_asset(); res != result::ok)
                        {
                            l->error("Failed to load level asset during processing command");
                            return res;
                        }
                        state->new_asset = ld.models.empty() ? asset_descriptions[0].asset : &level_asset;
                        return result::ok;
                    case editor_command::editor_command_type::add_to_level:
                        l->info("editor-command: adding to level.");
                        if (const auto res = add_model(cmd.id, cmd.mode_type_option); res != result::ok)
                        {
                            l->error(std::format("error adding model to level {}", static_cast<uint8_t>(res)));
                            return res;
                        }
                        break;
                    case editor_command::editor_command_type::remove_from_level:
                        l->info("editor-command: removing from level.");
                        if (const auto res = remove_model(cmd.id); res != result::ok)
                        {
                            l->error(std::format("error removing model from level {}", static_cast<uint8_t>(res)));
                            return res;
                        }
                        break;
                    case editor_command::editor_command_type::edit_level_node:
                        l->info(std::format("editor-command: editing level node {}. Translate: ({:.3f}, {:.3f}, {:.3f}) Scale: {:.3f} Yaw: {:.3f}",
                                            cmd.id,
                                            cmd.node_data.location[0],
                                            cmd.node_data.location[1],
                                            cmd.node_data.location[2],
                                            cmd.node_data.scale,
                                            cmd.node_data.yaw
                        ));
                        if (const auto res = edit_node(cmd.id, cmd.mode_type_option, cmd.node_data); res != result::ok)
                        {
                            l->error(std::format("error removing model from level {}", static_cast<uint8_t>(res)));
                            return res;
                        }
                        break;
                    }
                }
                if (!commands.commands.empty())
                {
                    // Update update state
                    state->current_level_data.static_models.clear();
                    state->current_level_data.mob_models.clear();
                    for (const auto& md : ld.models)
                    {
                        const auto model_type = static_cast<editor_command::model_type>(md.model_type);
                        level_data_model new_md = {.id = md.id, .name = md.name, .location = md.location, .scale = md.scale, .yaw = md.yaw, .model_type = model_type};
                        if (model_type == editor_command::model_type::mob_model)
                        {
                            state->current_level_data.mob_models.push_back(new_md);
                        }
                        if (model_type == editor_command::model_type::static_model)
                        {
                            state->current_level_data.static_models.push_back(new_md);
                        }
                    }
                }
                return result::ok;
            }
            // No assets initial load.
            if (const result res = read(); res != result::ok)
            {
                l->error("Failed to read level");
                return res;
            }
            if (const result res = load_asset(state); res != result::ok)
            {
                l->error("Failed to load asset");
                return res;
            }
            if (const result res = load_level_asset(); res != result::ok)
            {
                l->error("Failed to load level asset");
                return res;
            }
            {
                // Update post init state
                state->new_asset = ld.models.empty() ? asset_descriptions[0].asset : &level_asset;
                state->assets = asset_descriptions;
                state->current_level_data.static_models.clear();
                state->current_level_data.mob_models.clear();
                for (const auto& md : ld.models)
                {
                    const auto model_type = static_cast<editor_command::model_type>(md.model_type);
                    level_data_model new_md = {.id = md.id, .name = md.name, .location = md.location, .scale = md.scale, .yaw = md.yaw, .model_type = model_type};
                    if (model_type == editor_command::model_type::mob_model)
                    {
                        state->current_level_data.mob_models.push_back(new_md);
                    }
                    if (model_type == editor_command::model_type::static_model)
                    {
                        state->current_level_data.static_models.push_back(new_md);
                    }
                }
            }
            return result::ok;
        }

        result load_level_asset()
        {
            if (ld.models.empty()) return result::ok;
            if (origin_assets.empty())
            {
                l->error("No origin assets when loading level asset");
                return result::error;
            }
            // This constructs a new asset from the pieces of other assets as defined in the level json file.
            // Meshes, samplers, images, nodes, materials all have to be re-indexed so all indexes are pointing to the correct items
            // they were in the original asset.
            level_asset_builder lab{};
            level_asset = {};

            level_asset.shaders = origin_assets[0]->shaders; // Just use the first assets shaders, they're all the same right now.
            rosy_packager::scene new_scene{};
            new_scene.nodes.push_back(0); // Root node created below.
            level_asset.scenes.push_back(new_scene);

            rosy_packager::node root_node{};
            root_node.transform = {
                1.f, 0.f, 0.f, 0.f,
                0.f, 1.f, 0.f, 0.f,
                0.f, 0.f, 1.f, 0.f,
                0.f, 0.f, 0.f, 1.f
            };
            std::string root_name = "Root";
            std::ranges::copy(root_name, std::back_inserter(root_node.name));
            level_asset.nodes.push_back(root_node);

            rosy_packager::node mob_node{};
            mob_node.transform = {
                1.f, 0.f, 0.f, 0.f,
                0.f, 1.f, 0.f, 0.f,
                0.f, 0.f, 1.f, 0.f,
                0.f, 0.f, 0.f, 1.f
            };
            std::string mob_name = "mobs";
            std::ranges::copy(mob_name, std::back_inserter(mob_node.name));
            level_asset.nodes[0].child_nodes.push_back(static_cast<uint32_t>(level_asset.nodes.size()));
            level_asset.nodes.push_back(mob_node);

            rosy_packager::node static_node{};
            static_node.transform = {
                1.f, 0.f, 0.f, 0.f,
                0.f, 1.f, 0.f, 0.f,
                0.f, 0.f, 1.f, 0.f,
                0.f, 0.f, 0.f, 1.f
            };
            std::string static_name = "static";
            std::ranges::copy(static_name, std::back_inserter(static_node.name));
            level_asset.nodes[0].child_nodes.push_back(static_cast<uint32_t>(level_asset.nodes.size()));
            level_asset.nodes.push_back(static_node);

            // Used to traverse node children
            std::queue<uint32_t> node_descendants;
            // All the level models are iterated through.
            for (const auto& md : ld.models)
            {
                size_t asset_helper_index{0}; // The index of the asset helper in lab.
                l->info(std::format("recording level data model with id {}", md.id));
                // Identify the asset id for the model. model Ids are concatenated with ':' the first part is the asset id, which is its file path.
                // the subsequent parts are the node hierarchy in the mesh, the only thing needed is the last bit, which is the node name.
                // Node names must all be unique or this will fail and potentially get the wrong node.

                // This code starts by creating an asset helper to track all the reindexing.
                const std::string asset_id = md.id.substr(0, md.id.find(':'));
                bool found_asset{false};
                for (const auto& asset_helper : lab.assets)
                {
                    // Determine if we have previously added an asset helper for this models origin asset.
                    if (asset_helper.asset_id == asset_id)
                    {
                        l->info(std::format("found asset helper with id {}", asset_id));
                        found_asset = true;
                        break; // break here is important to not increment the asset_helper_index which is needed.
                    }
                    asset_helper_index += 1;
                }
                if (!found_asset)
                {
                    // We did not find an asset helper for the origin asset so adding it here.
                    level_asset_builder_source_asset_helper asset_helper{};
                    asset_helper.asset_id = asset_id;
                    // Find the origin assets index in the list of assets
                    bool found_asset_index{false};
                    for (const rosy_packager::asset* a : origin_assets)
                    {
                        if (a->asset_path == asset_id)
                        {
                            found_asset_index = true;
                            break;
                        }
                        asset_helper.rosy_package_asset_index += 1;
                    }
                    lab.assets.push_back(asset_helper);
                    if (!found_asset_index)
                    {
                        l->error(std::format("The asset must be found in the origin assets for a model. {}", md.id));
                        return result::error;
                    }
                    l->info(std::format("added asset helper with id {} and index {}", asset_id, asset_helper.rosy_package_asset_index));
                }
                {
                    l->info(std::format("using asset helper with id {} for {}", asset_helper_index, md.id));
                    // Keep a ref to the asset helper around and a pointer to the source asset, node name is found below
                    level_asset_builder_source_asset_helper& asset_helper = lab.assets[asset_helper_index];
                    const rosy_packager::asset* a = origin_assets[asset_helper.rosy_package_asset_index];
                    std::string model_node_name{};

                    // Having an asset helper to work with, find this models node name in its origin asset by splitting up the model id until at the end.
                    std::string id_parts = md.id.substr(asset_id.size() + 1);
                    while (id_parts.find(':') != std::string::npos)
                    {
                        const std::string model_name = id_parts.substr(0, id_parts.find(':'));
                        if (model_name.empty())
                        {
                            l->error(std::format("model_name empty this means an empty node name or invalid path is detected. Examine the asset. {}", md.id));
                            return result::error;
                        }
                        l->info(std::format("found model name: `{}`", model_name));
                        id_parts = id_parts.substr(model_name.size() + 1);
                        if (id_parts.empty())
                        {
                            l->error(std::format("id parts empty this means an empty node name or invalid path is detected. Examine the asset. {}", md.id));
                            return result::error;
                        }
                    }
                    {
                        // The parts left should be the model's node name in its origin asset, now find the models node index in its origin asset.
                        l->info(std::format("node name is: {}", id_parts));
                        model_node_name = id_parts;
                        size_t node_index{0};
                        for (const rosy_packager::node& n : a->nodes)
                        {
                            if (auto node_name = std::string(n.name.begin(), n.name.end()); model_node_name == node_name)
                            {
                                l->info(std::format("node index for {} in origin asset is: {}", md.id, node_index));
                                break;
                            }
                            node_index += 1;
                        }
                        if (node_index >= a->nodes.size())
                        {
                            l->error(std::format("Unable to find mesh with name {} in {}", model_node_name, md.id));
                            return result::error;
                        }
                        // The node's index and the index of all child nodes all the way down the graph need to be added to the new level asset and re-indexed.
                        {
                            if (!node_descendants.empty())
                            {
                                l->error(std::format("Unexpected descendants left in queue, load level asset is bugged. {}", md.id));
                                return result::error;
                            }
                            node_descendants.push(static_cast<uint32_t>(node_index));
                        }
                        bool is_world_node{true};
                        while (!node_descendants.empty())
                        {
                            uint32_t current_node_index = node_descendants.front();
                            node_descendants.pop();
                            l->info(std::format("traversing node {} for {}", current_node_index, md.id));

                            uint32_t destination_node_index{0};
                            {
                                // See if the node is already in the helper:
                                bool node_mapped{false};
                                for (const auto& nm : asset_helper.node_mappings)
                                {
                                    if (nm.source_index == current_node_index)
                                    {
                                        node_mapped = true;
                                        if (is_world_node)
                                        {
                                            // Even in this case we have seen the node before, add the top level model node to static or mob
                                            // the same model can be added multiple times.
                                            switch (static_cast<editor_command::model_type>(md.model_type))
                                            {
                                            case editor_command::model_type::no_model:
                                                l->error(std::format("invalid model type for {}", md.id));
                                                return result::error;
                                            case editor_command::model_type::mob_model:
                                                level_asset.nodes[1].child_nodes.push_back(nm.destination_index);
                                                break;
                                            case editor_command::model_type::static_model:
                                                level_asset.nodes[2].child_nodes.push_back(nm.destination_index);
                                                break;
                                            }
                                        }
                                        break;
                                    }
                                }
                                if (node_mapped)
                                {
                                    // This node has been previously mapped.
                                    continue;
                                }

                                // It's not, so map it.
                                {
                                    destination_node_index = static_cast<uint32_t>(level_asset.nodes.size());
                                    level_asset_builder_index_map nm{
                                        .source_index = current_node_index,
                                        .destination_index = destination_node_index,
                                    };
                                    l->info(std::format("current_node_index mapped to {} destination_index {} in {}", current_node_index, destination_node_index, md.id));
                                    asset_helper.node_mappings.push_back(nm);
                                    rosy_packager::node source_node = a->nodes[current_node_index];
                                    rosy_packager::node new_destination_node{};
                                    new_destination_node.name = source_node.name;
                                    {
                                        // Every node needs to know its world node's parent's world transform
                                        new_destination_node.world_translate = md.location;
                                        new_destination_node.world_scale = md.scale;
                                        new_destination_node.world_yaw = md.yaw;
                                    }
                                    new_destination_node.coordinate_system = a->asset_coordinate_system;
                                    new_destination_node.is_world_node = is_world_node;
                                    new_destination_node.transform = source_node.transform;
                                    new_destination_node.child_nodes = source_node.child_nodes; // These are remapped below.
                                    new_destination_node.mesh_id = UINT32_MAX; // This is remapped below.
                                    level_asset.nodes.push_back(new_destination_node);
                                }
                            }
                            if (is_world_node)
                            {
                                // Add the top level world model node to static or mob.
                                switch (static_cast<editor_command::model_type>(md.model_type))
                                {
                                case editor_command::model_type::no_model:
                                    l->error(std::format("invalid world model type for {}", md.id));
                                    return result::error;
                                case editor_command::model_type::mob_model:
                                    level_asset.nodes[1].child_nodes.push_back(destination_node_index);
                                    break;
                                case editor_command::model_type::static_model:
                                    level_asset.nodes[2].child_nodes.push_back(destination_node_index);
                                    break;
                                }
                                is_world_node = false;
                            }
                            // Get a reference to the destination node to change mesh index. Children have to be fully re-indexed in this queue before they can be remapped.
                            {
                                rosy_packager::node& destination_node = level_asset.nodes[destination_node_index];
                                // All these nodes have to have a mesh. I need to track that still in the asset viewer UI so nodes without meshes don't show up and can't be added to level data.
                                const uint32_t current_mesh_index = a->nodes[current_node_index].mesh_id;
                                uint32_t destination_mesh_index{0};
                                // See if the node's mesh is already in the helper:
                                bool mesh_mapped{false};
                                for (const auto& mm : asset_helper.mesh_mappings)
                                {
                                    if (mm.source_index == current_mesh_index)
                                    {
                                        mesh_mapped = true;
                                        // This mesh has been previously mapped, so update the destination's node's mesh id to track that.
                                        destination_mesh_index = mm.destination_index;
                                        destination_node.mesh_id = destination_mesh_index;
                                        break;
                                    }
                                }
                                if (mesh_mapped)
                                {
                                    // Mesh was previously mapped and does not need to be mapped again.
                                    continue;
                                }
                                {
                                    {
                                        destination_mesh_index = static_cast<uint32_t>(level_asset.meshes.size());
                                        level_asset_builder_index_map mm{
                                            .source_index = current_mesh_index,
                                            .destination_index = destination_mesh_index,
                                        };
                                        l->info(std::format("current_mesh_index mapped to {} destination_mesh_index {} in {}", current_mesh_index, destination_mesh_index, md.id));
                                        destination_node.mesh_id = destination_mesh_index;
                                        asset_helper.mesh_mappings.push_back(mm);
                                        // Add the new destination mesh
                                        rosy_packager::mesh source_mesh = a->meshes[current_mesh_index];
                                        rosy_packager::mesh new_destination_mesh{};
                                        new_destination_mesh.positions = source_mesh.positions;
                                        new_destination_mesh.indices = source_mesh.indices;
                                        new_destination_mesh.surfaces = source_mesh.surfaces;
                                        level_asset.meshes.push_back(new_destination_mesh);
                                    }

                                    // Map all the materials images sand samplers
                                    for (rosy_packager::surface& surface : level_asset.meshes[destination_mesh_index].surfaces)
                                    {
                                        const uint32_t current_mat_index = surface.material;
                                        uint32_t destination_mat_index{0};

                                        bool material_mapped{false};
                                        for (const auto& mat_m : asset_helper.material_mappings)
                                        {
                                            if (mat_m.source_index == current_mat_index)
                                            {
                                                material_mapped = true;
                                                // This mesh has been previously mapped, so update the destination's node's mesh id to track that.
                                                destination_mat_index = mat_m.destination_index;
                                                surface.material = destination_mat_index;
                                                break;
                                            }
                                        }
                                        if (material_mapped)
                                        {
                                            // Material was previously mapped, no need to remap.
                                            continue;
                                        }
                                        rosy_packager::material source_mat = a->materials[current_mat_index];
                                        {
                                            destination_mat_index = static_cast<uint32_t>(level_asset.materials.size());
                                            level_asset_builder_index_map mat_m{
                                                .source_index = current_mat_index,
                                                .destination_index = destination_mat_index,
                                            };
                                            surface.material = destination_mat_index;
                                            l->info(std::format("current_mat_index mapped to {} destination_mat_index {} in {}", current_mat_index, destination_mat_index, md.id));
                                            asset_helper.material_mappings.push_back(mat_m);
                                            // Add the new destination mat
                                            rosy_packager::material destination_mat{};
                                            destination_mat.double_sided = source_mat.double_sided;
                                            destination_mat.base_color_factor = source_mat.base_color_factor;
                                            destination_mat.metallic_factor = source_mat.metallic_factor;
                                            destination_mat.roughness_factor = source_mat.roughness_factor;
                                            destination_mat.color_image_index = UINT32_MAX;
                                            destination_mat.color_sampler_index = UINT32_MAX;
                                            destination_mat.alpha_mode = source_mat.alpha_mode;
                                            destination_mat.alpha_cutoff = source_mat.alpha_cutoff;
                                            destination_mat.normal_image_index = UINT32_MAX;
                                            destination_mat.normal_sampler_index = UINT32_MAX;
                                            level_asset.materials.push_back(destination_mat);
                                        }
                                        rosy_packager::material& destination_map = level_asset.materials[destination_mat_index];
                                        // map the color_image_index
                                        if (source_mat.color_image_index != UINT32_MAX)
                                        {
                                            const uint32_t current_image_index = source_mat.color_image_index;
                                            uint32_t destination_image_index{0};

                                            bool image_mapped{false};
                                            for (const auto& im : asset_helper.image_mappings)
                                            {
                                                if (im.source_index == current_image_index)
                                                {
                                                    image_mapped = true;
                                                    destination_image_index = im.destination_index;
                                                    destination_map.color_image_index = destination_image_index;
                                                    l->info(std::format("color_image_index previously mapped to {} destination_image_index {} in {} in asset_helper_index {}",
                                                                        current_image_index, destination_image_index, md.id, asset_helper_index));
                                                    break;
                                                }
                                            }
                                            if (!image_mapped)
                                            {
                                                destination_image_index = static_cast<uint32_t>(level_asset.images.size());
                                                level_asset_builder_index_map img_m{
                                                    .source_index = current_image_index,
                                                    .destination_index = destination_image_index,
                                                };
                                                destination_map.color_image_index = destination_image_index;
                                                l->info(std::format("color_image_index mapped to {} destination_image_index {} in {} in asset_helper_index {}", current_image_index,
                                                                    destination_image_index, md.id, asset_helper_index));
                                                asset_helper.image_mappings.push_back(img_m);
                                                rosy_packager::image source_image = a->images[current_image_index];
                                                rosy_packager::image destination_image{};
                                                destination_image.name = source_image.name;
                                                destination_image.image_type = source_image.image_type;
                                                level_asset.images.push_back(destination_image);
                                            }
                                        }
                                        // map the color_sampler_index
                                        if (source_mat.color_sampler_index != UINT32_MAX)
                                        {
                                            const uint32_t current_sampler_index = source_mat.color_sampler_index;
                                            uint32_t destination_sampler_index{0};

                                            bool sampler_mapped{false};
                                            for (const auto& sm : asset_helper.sampler_mappings)
                                            {
                                                if (sm.source_index == current_sampler_index)
                                                {
                                                    sampler_mapped = true;
                                                    destination_sampler_index = sm.destination_index;
                                                    destination_map.color_sampler_index = destination_sampler_index;
                                                    break;
                                                }
                                            }
                                            if (!sampler_mapped)
                                            {
                                                destination_sampler_index = static_cast<uint32_t>(level_asset.images.size());
                                                level_asset_builder_index_map img_m{
                                                    .source_index = current_sampler_index,
                                                    .destination_index = destination_sampler_index,
                                                };
                                                destination_map.color_sampler_index = destination_sampler_index;
                                                l->info(std::format("color_sampler_index mapped to {} destination_sampler_index {} in {} in asset_helper_index {}",
                                                                    current_sampler_index, destination_sampler_index, md.id, asset_helper_index));
                                                asset_helper.sampler_mappings.push_back(img_m);
                                                rosy_packager::sampler source_sampler = a->samplers[current_sampler_index];
                                                rosy_packager::sampler destination_sampler{};
                                                destination_sampler.min_filter = source_sampler.min_filter;
                                                destination_sampler.mag_filter = source_sampler.mag_filter;
                                                destination_sampler.wrap_s = source_sampler.wrap_s;
                                                destination_sampler.wrap_t = source_sampler.wrap_t;
                                                level_asset.samplers.push_back(destination_sampler);
                                            }
                                        }
                                        // map the normal_image_index
                                        if (source_mat.normal_image_index != UINT32_MAX)
                                        {
                                            const uint32_t current_image_index = source_mat.normal_image_index;
                                            uint32_t destination_image_index{0};

                                            bool image_mapped{false};
                                            for (const auto& im : asset_helper.image_mappings)
                                            {
                                                if (im.source_index == current_image_index)
                                                {
                                                    image_mapped = true;
                                                    destination_image_index = im.destination_index;
                                                    destination_map.normal_image_index = destination_image_index;
                                                    break;
                                                }
                                            }
                                            if (!image_mapped)
                                            {
                                                destination_image_index = static_cast<uint32_t>(level_asset.images.size());
                                                level_asset_builder_index_map img_m{
                                                    .source_index = current_image_index,
                                                    .destination_index = destination_image_index,
                                                };
                                                destination_map.normal_image_index = destination_image_index;
                                                l->info(std::format("normal_image_index mapped to {} destination_image_index {} in {} in asset_helper_index {}",
                                                                    current_image_index, destination_image_index, md.id, asset_helper_index));
                                                asset_helper.image_mappings.push_back(img_m);
                                                rosy_packager::image source_image = a->images[current_image_index];
                                                rosy_packager::image destination_image{};
                                                destination_image.name = source_image.name;
                                                destination_image.image_type = source_image.image_type;
                                                level_asset.images.push_back(destination_image);
                                            }
                                        }
                                        // map the normal_sampler_index
                                        if (source_mat.normal_sampler_index != UINT32_MAX)
                                        {
                                            const uint32_t current_sampler_index = source_mat.normal_sampler_index;
                                            uint32_t destination_sampler_index{0};

                                            bool sampler_mapped{false};
                                            for (const auto& sm : asset_helper.sampler_mappings)
                                            {
                                                if (sm.source_index == current_sampler_index)
                                                {
                                                    sampler_mapped = true;
                                                    destination_sampler_index = sm.destination_index;
                                                    destination_map.normal_sampler_index = destination_sampler_index;
                                                    break;
                                                }
                                            }
                                            if (!sampler_mapped)
                                            {
                                                destination_sampler_index = static_cast<uint32_t>(level_asset.images.size());
                                                level_asset_builder_index_map img_m{
                                                    .source_index = current_sampler_index,
                                                    .destination_index = destination_sampler_index,
                                                };
                                                destination_map.normal_sampler_index = destination_sampler_index;
                                                l->info(std::format("normal_sampler_index mapped to {} destination_sampler_index {} in {} in asset_helper_index {}",
                                                                    current_sampler_index, destination_sampler_index, md.id, asset_helper_index));
                                                asset_helper.sampler_mappings.push_back(img_m);
                                                rosy_packager::sampler source_sampler = a->samplers[current_sampler_index];
                                                rosy_packager::sampler destination_sampler{};
                                                destination_sampler.min_filter = source_sampler.min_filter;
                                                destination_sampler.mag_filter = source_sampler.mag_filter;
                                                destination_sampler.wrap_s = source_sampler.wrap_s;
                                                destination_sampler.wrap_t = source_sampler.wrap_t;
                                                level_asset.samplers.push_back(destination_sampler);
                                            }
                                        }
                                    }
                                }
                            }
                            for (const uint32_t child : a->nodes[current_node_index].child_nodes) node_descendants.push(child);
                            is_world_node = false;
                        }
                    }
                }
            }


            // Now need to fix all the child nodes. For each node mapping added fix the child nodes.
            for (const auto& asset_helper : lab.assets)
            {
                for (const level_asset_builder_index_map& parent_node_mapping : asset_helper.node_mappings)
                {
                    const rosy_packager::asset* a = origin_assets[asset_helper.rosy_package_asset_index];
                    rosy_packager::node& destination_node = level_asset.nodes[parent_node_mapping.destination_index];
                    const size_t num_child_nodes_expected = destination_node.child_nodes.size();
                    if (a->nodes[parent_node_mapping.source_index].child_nodes.size() != num_child_nodes_expected)
                    {
                        l->error(std::format("failed to map node children, was {} but expected {}", destination_node.child_nodes.size(),
                                             a->nodes[parent_node_mapping.source_index].child_nodes.size()));
                        return result::error;
                    }
                    l->info(std::format("Have {} child nodes to remap for {}", num_child_nodes_expected, asset_helper.asset_id));
                    if (num_child_nodes_expected == 0) continue;
                    const std::vector<uint32_t> source_child_nodes = destination_node.child_nodes;
                    destination_node.child_nodes.clear();
                    for (const uint32_t source_child_node_index : source_child_nodes)
                    {
                        for (const level_asset_builder_index_map& child_node_mapping : asset_helper.node_mappings)
                        {
                            if (child_node_mapping.source_index == source_child_node_index)
                            {
                                destination_node.child_nodes.push_back(child_node_mapping.destination_index);
                                l->info(std::format("remapped child node from {} to {} in {}", source_child_node_index, child_node_mapping.destination_index,
                                                    asset_helper.asset_id));
                                break;
                            }
                        }
                    }
                    if (destination_node.child_nodes.size() != num_child_nodes_expected)
                    {
                        l->error(std::format("failed to reindex node children, was {} but expected {}", destination_node.child_nodes.size(), num_child_nodes_expected));
                        return result::error;
                    }
                    l->info(std::format("finished remapping node {} in {}", parent_node_mapping.source_index, asset_helper.asset_id));
                }
            }
            l->info("finished remapping level data models");
            return result::ok;
        }

        result load_asset([[maybe_unused]] level_editor_state* state)
        {
            std::array<std::string, 5> asset_paths{
                R"(..\assets\houdini\exports\Box_002\Box_002.rsy)",
                R"(..\assets\sponza\sponza.rsy)",
                R"(..\assets\cornell_dragons\cornell_dragons.rsy)",
                R"(..\assets\two_cubes\two_cubes.rsy)",
                R"(..\assets\deccer_cubes\SM_Deccer_Cubes_Textured_Complex.rsy)",
            };
            for (std::string& path : asset_paths)
            {
                rosy_packager::asset* a;
                if (a = new(std::nothrow) rosy_packager::asset; a == nullptr)
                {
                    l->error("asset allocation failed");
                    return result::allocation_failure;
                }
                {
                    a->asset_path = path;
                    {
                        if (const auto res = a->read(l); res != result::ok)
                        {
                            l->error(std::format("Failed to read the assets for {}!", path));
                            return result::error;
                        }
                    }
                    rosy_packager::shader new_shader{};
                    new_shader.path = "../shaders/out/basic.spv";
                    a->shaders.push_back(new_shader);
                    {
                        if (const auto res = a->read_shaders(l); res != result::ok)
                        {
                            l->error("Failed to read shaders!");
                            return result::error;
                        }
                    }
                }
                const std::filesystem::path asset_path{a->asset_path};

                asset_description desc{};
                desc.id = asset_path.string();
                desc.name = asset_path.filename().string();
                desc.asset = static_cast<const void*>(a);
                origin_assets.push_back(a);

                load_models(desc, a);
                asset_descriptions.push_back(desc);
            }
            return result::ok;
        }

        static result load_models([[maybe_unused]] asset_description& desc,
                                  [[maybe_unused]] const rosy_packager::asset* new_asset)
        {
            // Traverse the assets to construct model descriptions.
            std::vector<model_description> models;
            const size_t root_scene_index = static_cast<size_t>(new_asset->root_scene);
            if (new_asset->scenes.size() <= root_scene_index) return result::invalid_argument;

            const auto& scene = new_asset->scenes[root_scene_index];
            if (scene.nodes.empty()) return result::invalid_argument;

            std::queue<stack_item> queue;

            for (const auto& node_index : scene.nodes)
            {
                const rosy_packager::node new_node = new_asset->nodes[node_index];

                queue.push({
                    .id = desc.id,
                    .stack_node = new_node,
                });
            }

            while (!queue.empty())
            {
                stack_item queue_item = queue.front();
                queue.pop();

                model_description model_desc;
                model_desc.name = std::string(queue_item.stack_node.name.begin(), queue_item.stack_node.name.end());
                model_desc.id = std::format("{}:{}", queue_item.id, model_desc.name);
                desc.models.push_back(model_desc);

                for (const size_t child_index : queue_item.stack_node.child_nodes)
                {
                    const rosy_packager::node new_node = new_asset->nodes[child_index];

                    queue.push({
                        .id = model_desc.id,
                        .stack_node = new_node,
                    });
                }
            }

            return result::ok;
        }
    };

    editor_manager* em{nullptr};
}


// ReSharper disable once CppMemberFunctionMayBeStatic
result editor::init(log* new_log, [[maybe_unused]] config new_cfg)
{
    if (em)
    {
        em->deinit();
    }
    if (em = new(std::nothrow) editor_manager; em == nullptr)
    {
        new_log->error("editor_manager allocation failed");
        return result::allocation_failure;
    }
    if (const result res = em->init(new_log); res != result::ok)
    {
        new_log->error("editor_manager allocation failed");
        return res;
    }
    new_log->info("Hello world!");
    return result::ok;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void editor::deinit()
{
    em->deinit();
    delete em;
    em = nullptr;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
result editor::process([[maybe_unused]] const level_editor_commands& commands,
                       [[maybe_unused]] level_editor_state* state)
{
    return em->process(commands, state);
}

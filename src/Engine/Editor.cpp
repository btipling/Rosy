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
        float yaw{0.f};
        uint32_t model_type{0};
    };

    struct level_data
    {
        std::vector<level_data_model> models;
    };

    void to_json(json& j, const level_data_model& model)
    {
        j = json{
            {"id", model.id},
            {"name", model.name},
            {"location", model.location},
            {"yaw", model.yaw},
            {"model_type", model.model_type},
        };
    }

    void from_json(const json& j, level_data_model& p)
    {
        j.at("id").get_to(p.id);
        j.at("name").get_to(p.name);
        j.at("location").get_to(p.location);
        j.at("yaw").get_to(p.yaw);
        j.at("model_type").get_to(p.model_type);
    }

    void to_json(json& j, const level_data& l)
    {
        j = json{{"models", l.models}};
    }

    void from_json(const json& j, level_data& l)
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

    struct editor_manager
    {
        rosy::log* l{nullptr};
        std::vector<asset_description> assets;
        rosy_editor::level_data ld;

        result init(rosy::log* new_log)
        {
            l = new_log;
            return result::ok;
        }

        void deinit()
        {
            l = nullptr;
            for (asset_description& desc : assets)
            {
                const auto a = static_cast<const rosy_packager::asset*>(desc.asset);
                delete a;
                desc.asset = nullptr;
            }
        }

        [[nodiscard]] result add_model(std::string id, editor_command::model_type type)
        {
            for (const auto& a : assets)
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
                l->info(std::format("id: {} name: {} location: ({:.3f}, {:.3f}, {:.3f}) yaw: {:.3f}", m.id, m.name, m.location[0], m.location[1], m.location[2], m.yaw));
            }
            return result::ok;
        }

        [[nodiscard]] result process(const level_editor_commands& commands, level_editor_state* state)
        {
            state->new_asset = nullptr;
            if (!assets.empty())
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
                        for (const auto& a : assets)
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
                        break;
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
                    }
                }
                if (!commands.commands.empty()) {
                    // Update update state
                    state->current_level_data.static_models.clear();
                    state->current_level_data.mob_models.clear();
                    for (const auto& md : ld.models)
                    {
                        const auto model_type = static_cast<editor_command::model_type>(md.model_type);
                        level_data_model new_md = {.id = md.id, .name = md.name, .location = md.location, .yaw = md.yaw, .model_type = model_type};
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
            if (const result res = load_asset(state); res != result::ok)
            {
                l->error("Failed to load asset");
                return res;
            }
            {
                // Update post init state
                state->assets = assets;
                state->current_level_data.static_models.clear();
                state->current_level_data.mob_models.clear();
                for (const auto& md : ld.models)
                {
                    const auto model_type = static_cast<editor_command::model_type>(md.model_type);
                    level_data_model new_md = {.id = md.id, .name = md.name, .location = md.location, .yaw = md.yaw, .model_type = model_type};
                    if (model_type == editor_command::model_type::static_model)
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

        result load_asset(level_editor_state* state)
        {
            std::array<std::string, 3> asset_paths{
                R"(..\assets\houdini\exports\Box_002\Box_002.rsy)",
                R"(..\assets\sponza\sponza.rsy)",
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
                load_models(desc, a);
                assets.push_back(desc);
            }
            state->new_asset = assets[0].asset;
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
result editor::process([[maybe_unused]] const level_editor_commands commands,
                       [[maybe_unused]] level_editor_state* state)
{
    return em->process(commands, state);
}

#include "Editor.h"
#include "../Packager/Asset.h"

using namespace rosy;

namespace
{
    struct editor_manager
    {
        rosy::log* l{nullptr};
        std::vector<asset_description> assets;

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

        result process([[maybe_unused]] level_editor_commands commands, level_editor_state* state)
        {
            state->new_asset = nullptr;
            if (!assets.empty()) return result::ok;
            return load_asset(state);
        }

        result load_asset([[maybe_unused]] level_editor_state* state)
        {
            rosy_packager::asset* a;
            if (a = new(std::nothrow) rosy_packager::asset; a == nullptr)
            {
                l->error("asset allocation failed");
                return result::allocation_failure;
            }
            {
                //a.asset_path = "..\\assets\\sponza\\sponza.rsy";
                a->asset_path = "..\\assets\\houdini\\exports\\Box_002\\Box_002.rsy";
                //a.asset_path = "..\\assets\\deccer_cubes\\SM_Deccer_Cubes_Textured_Complex.rsy";
                {
                    if (const auto res = a->read(l); res != result::ok)
                    {
                        l->error("Failed to read the assets!");
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
            asset_description desc{};
            desc.asset = static_cast<const void*>(a);
            assets.push_back(desc);
            state->new_asset = desc.asset;
            return result::ok;
        }
    };

    editor_manager* em{nullptr};
}

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
    em = nullptr;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
result editor::process([[maybe_unused]] const level_editor_commands commands,
                       [[maybe_unused]] level_editor_state* state)
{
    return em->process(commands, state);
}

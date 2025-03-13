#include "pch.h"
#include "Gltf.h"
#include "FBX.h"

using namespace rosy_packager;

namespace {
    int load_gltf(rosy::log* l, const std::filesystem::path& source_path)
    {
        const auto start = std::chrono::system_clock::now();
        std::filesystem::path output_path{ source_path };
        output_path.replace_extension(".rsy");
        l->info(std::format("Parsing {} as {}", source_path.string(), output_path.string()));
        gltf g{};
        {
            asset a{};
            a.asset_path = output_path.string();
            g.source_path = source_path.string();
            g.gltf_asset = a;
        }
        gltf_config gltf_cfg{
            .condition_images = true,
            .use_mikktspace = true,
        };
        if (const auto res = g.import(l, gltf_cfg); res != rosy::result::ok)
        {
            l->error(std::format("Error importing gltf {}", static_cast<uint8_t>(res)));
            return EXIT_FAILURE;
        }
        if (const auto res = g.gltf_asset.write(l); res != rosy::result::ok)
        {
            return EXIT_FAILURE;
        }
        int mi{ 0 };
        constexpr int max_pos{ 1 };
        for (const auto& m : g.gltf_asset.meshes)
        {
            int pi{ 0 };
            for (const auto& p : m.positions)
            {
                l->debug(std::format(
                    "mesh: {: 3d} vertex: {: 6d}: ({:+.3f}, {:+.3f}, {:+.3f}) | normal: ({:+.3f}, {:+.3f}, {:+.3f}) | tangent:  ({:+.3f}, {:+.3f}, {:+.3f}, {:+.3f}) | color: ({:+.3f}, {:+.3f}, {:+.3f}, {:+.3f}) | texture coordinates: ({:+.3f}, {:+.3f})",
                    mi, pi,
                    p.vertex[0], p.vertex[1], p.vertex[2],
                    p.normal[0], p.normal[1], p.normal[2],
                    p.tangents[0], p.tangents[1], p.tangents[2], p.tangents[3],
                    p.color[0], p.color[1], p.color[2], p.color[3],
                    p.texture_coordinates[0], p.texture_coordinates[1]
                ));
                pi += 1;
                if (pi >= max_pos)
                {
                    break;
                }
            }
            mi += 1;
        }

        const auto end = std::chrono::system_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        l->info(std::format("Finished packaging. Took {}ms", static_cast<double>(elapsed.count()) / 1000.0l));
        return 0;
    }


    int load_fbx(const rosy::log* l, const std::filesystem::path& source_path)
    {
        const auto start = std::chrono::system_clock::now();
        std::filesystem::path output_path{ source_path };
        output_path.replace_extension(".rsy");
        l->info(std::format("Parsing {} as {}", source_path.string(), output_path.string()));
        fbx f{};
        {
            asset a{};
            a.asset_path = output_path.string();
            f.source_path = source_path.string();
            f.fbx_asset = a;
        }
        fbx_config fbx_cfg{
            .condition_images = true,
            .use_mikktspace = true,
        };
        if (const auto res = f.import(l, fbx_cfg); res != rosy::result::ok)
        {
            l->error(std::format("Error importing fbx {}", static_cast<uint8_t>(res)));
            return EXIT_FAILURE;
        }
        if (const auto res = f.fbx_asset.write(l); res != rosy::result::ok)
        {
            return EXIT_FAILURE;
        }
        int mi{ 0 };
        constexpr int max_pos{ 1000 };
        for (const auto& m : f.fbx_asset.meshes)
        {
            int pi{ 0 };
            for (const auto& p : m.positions)
            {
                l->info(std::format(
                    "mesh: {: 3d} vertex: {: 6d}: ({:+.3f}, {:+.3f}, {:+.3f}) | normal: ({:+.3f}, {:+.3f}, {:+.3f}) | tangent:  ({:+.3f}, {:+.3f}, {:+.3f}, {:+.3f}) | color: ({:+.3f}, {:+.3f}, {:+.3f}, {:+.3f}) | texture coordinates: ({:+.3f}, {:+.3f})",
                    mi, pi,
                    p.vertex[0], p.vertex[1], p.vertex[2],
                    p.normal[0], p.normal[1], p.normal[2],
                    p.tangents[0], p.tangents[1], p.tangents[2], p.tangents[3],
                    p.color[0], p.color[1], p.color[2], p.color[3],
                    p.texture_coordinates[0], p.texture_coordinates[1]
                ));
                pi += 1;
                if (pi >= max_pos)
                {
                    break;
                }
            }
            mi += 1;
        }

        const auto end = std::chrono::system_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        l->info(std::format("Finished packaging. Took {}ms", static_cast<double>(elapsed.count()) / 1000.0l));
        return 0;
    }
}

int main(const int argc, char* argv[])
{
    rosy::log l{};
#ifdef ROSY_LOG_LEVEL_DEBUG
    l.level = rosy::log_level::debug;
#endif
    l.info("Starting packager");
    l.debug(std::format("Received {} args.", argc));
    const auto cwd = std::filesystem::current_path();
    for (int i = 0; i < argc; i++)
    {
        l.debug(std::format("arg {}: {}", i, argv[i]));
    }
    if (argc <= 1)
    {
        l.error("Need to provide a relative or absolute path to a gltf file");
        return EXIT_FAILURE;
    }
    std::filesystem::path source_path{ argv[1] };
    if (!source_path.has_extension())
    {
        l.error("Need to provide a path to gltf file with the gltf extension, glb is not supported.");
        return EXIT_FAILURE;
    }
    if (source_path.extension() == ".fbx")
    {
        l.info("importing an fbx file");
        if (!source_path.is_absolute())
        {
            source_path = std::filesystem::path{ std::format("{}\\{}", cwd.string(), source_path.string()) };
        }
        return load_fbx(&l, source_path);
    }
    if (source_path.extension() != ".gltf")
    {
        l.error(std::format("Received a path without a gltf extension, glb is not supported. Found {}",
            source_path.extension().string()));
        return EXIT_FAILURE;
    }
    if (!source_path.is_absolute())
    {
        source_path = std::filesystem::path{ std::format("{}\\{}", cwd.string(), source_path.string()) };
    }
    return load_gltf(&l, source_path);
};

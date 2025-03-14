vk_sdk = os.getenv("VULKAN_SDK")
nvtt_path = os.getenv("NVTT_PATH")
fbx_sdk = os.getenv("FBX_SDK")

debug_configurations = "configurations:Debug or configurations:RenderDoc or configurations:Clang or configurations:Sanitize"
release_configurations = "configurations:Release"
-- warnings disabled for third party code
-- warnings disabled for clang as they conflict with msbuild
warnings_disabled = "files:libs/**.c or files:libs/**.cpp or configurations:Clang"
warnings_enabled = "not configurations:Clang"
    -- precompiled headers disabled for third party code
pch_disabled = "files:libs/**.c or files:libs/**.cpp"

workspace "Rosy"
    configurations { "Debug", "Release", "RenderDoc", "Clang", "Sanitize" }
    -- shared configurations
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++23"
    targetdir "bin/%{cfg.buildcfg}"
    architecture("x86_64")
    flags { "MultiProcessorCompile" }
    -- debug configurations
    filter(debug_configurations)
        defines { "DEBUG" }
        symbols "On"
    filter {}
    -- release configurations
    filter(release_configurations)
        defines { "NDEBUG" }
        optimize "On"
    filter {}
    -- Precompiled headers
    pchheader "pch.h"
    pchsource "pch.cpp"
    includedirs { "." }
    files { "pch.h", "pch.cpp" }
    -- clang related specific options
    filter "configurations:Clang"
        toolset("clang")
    filter {}
    -- precompiled headers
    filter(pch_disabled)
        flags {"NoPCH"}
    filter {}
    -- warnings
    filter(warnings_enabled)
        warnings "Extra"
        fatalwarnings "All"
    filter {}
    filter(warnings_disabled)
        warnings "Off"
    filter {}
    -- Sanitize
    filter "configurations:Sanitize"
        sanitize { "Address" }
        editandcontinue "Off"
    filter {}

project "Engine"
    debugdir "./Engine/"
    -- source files
    files { "Engine/**.h", "Engine/**.cpp" }
    files { "Asset/**.h", "Asset/**.cpp" }
    files { "Logger/**.h", "Logger/**.cpp" }
    files { "libs/imgui/**.h", "libs/imgui/**.cpp" }
    files { "libs/json/single_include/nlohmann/json.hpp" }
    -- shader files and scripts
    files { "shaders/*.slang", "shaders/*.ps1" }
    -- libraries included as files
    files { "libs/Volk.cpp" }
    files { "libs/VMA.cpp" }
    -- include directories
    includedirs { "libs/SDL/include/" }
    includedirs { vk_sdk .. "/Include/" }
    includedirs { "libs/imgui/" }
    includedirs { "libs/tracy/" }
    includedirs { "libs/" }
    includedirs { "libs/json/single_include/" }
    includedirs { "libs/flecs/include/" }
    -- linking
    links { "SDL3" }
    links { "flecs" }
    -- library directories
    libdirs { vk_sdk .. "/Lib/" }
    filter(debug_configurations)
        libdirs { "libs/SDL/build/Debug" }
        libdirs { "libs/flecs/out/Debug" }
    filter {}
    filter(release_configurations)
        libdirs { "libs/SDL/build/Release" }
        libdirs { "libs/flecs/out/Release" }
    filter {}
    -- defines
    defines { "SIMDJSON_EXCEPTIONS=OFF" }
    filter "configurations:RenderDoc"
        defines { "RENDERDOC" }
    filter {}
    -- build hooks
    buildmessage "Compiling shaders"
    prebuildcommands {
        "Powershell -File %{prj.location}/shaders/script_compile.ps1 ./shaders/"
    }

project "Packager"
    debugdir "./Packager/"
    -- source files
    files { "Packager/**.h", "Packager/**.cpp" }
    files { "Asset/**.h", "Asset/**.cpp" }
    files { "Logger/**.h", "Logger/**.cpp" }
    files { "libs/MikkTSpace/mikktspace.c" }
    -- include directories
    includedirs { vk_sdk .. "/Include/" }
    includedirs { "libs/fastgltf/include/" }
    includedirs { "\"" .. nvtt_path .. "/include/\"" }
    includedirs { "\"" .. fbx_sdk .. "/include/\"" }
    includedirs { "libs/stb/" }
    includedirs { "libs/MikkTSpace/" }
    includedirs { "libs/MikkTSpace/" }
    includedirs { "libs/meshoptimizer/src" }
    -- linking
    links { "fastgltf" }
    links { "nvtt30205" }
    links { "libfbxsdk" }
    links ( "meshoptimizer" )
    -- library directories
    libdirs { "\"" .. nvtt_path .. "/lib/x64-v142/\"" }
    filter(debug_configurations)
        libdirs { "libs/fastgltf/build/Debug" }
        libdirs { "\"" .. fbx_sdk .. "/lib/x64/debug/\"" }
        libdirs { "libs/meshoptimizer/build/Debug" }
    filter(release_configurations)
        libdirs { "libs/fastgltf/build/Release" }
        libdirs { "\"" .. fbx_sdk .. "/lib/x64/release/\"" }
        libdirs { "libs/meshoptimizer/build/Release" }

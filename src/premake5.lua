workspace "Rosy"
    configurations { "Debug", "Release", "RenderDoc", "Clang" }
    filter "not configurations:Clang"
        warnings "Extra"
        fatalwarnings "All"
    filter {}

vk_sdk = os.getenv("VULKAN_SDK")
nvtt_path = os.getenv("NVTT_PATH")

project "Engine"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++23"
    targetdir "bin/%{cfg.buildcfg}"
    architecture("x86_64")
    filter "configurations:Clang"
        toolset("clang")
    filter {}
    debugdir "./Engine/"
    flags { "MultiProcessorCompile" }

    links { "SDL3" }
    links { "flecs" }
    links { "nvtt30205" }

    includedirs { "libs/SDL/include/" }
    includedirs { vk_sdk .. "/Include/" }
    includedirs { "libs/imgui/" }
    includedirs { "libs/tracy/" }
    includedirs { "libs/" }
    includedirs { "libs/json/single_include/" }
    includedirs { "\"" .. nvtt_path .. "/include/\"" }
    includedirs { "libs/flecs/include/" }

    libdirs { "libs/SDL/build/Debug" }
    libdirs { vk_sdk .. "/Lib/" }
    libdirs { "libs/flecs/out/Debug" }
    libdirs { "\"" .. nvtt_path .. "/lib/x64-v142/\"" }

    defines { "SIMDJSON_EXCEPTIONS=OFF" }
    filter "configurations:RenderDoc"
        defines { "RENDERDOC" }
    filter {}

    files { "Engine/**.h", "Engine/**.cpp" }
    files { "Packager/Asset.h", "Packager/Asset.cpp" }
    files { "libs/imgui/**.h", "libs/imgui/**.cpp" }
    files { "libs/Volk.cpp" }
    files { "libs/VMA.cpp" }
    files { "libs/json/single_include/nlohmann/json.hpp" }
    files { "shaders/*.slang", "shaders/*.ps1" }

    defines { "KHRONOS_STATIC" }
    filter { "configurations:Debug or configurations:RenderDoc or configurations:Clang" }
        defines { "DEBUG" }
        symbols "On"
    filter {}

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
    filter {}

    buildmessage "Compiling shaders?"
    prebuildcommands {
        "Powershell -File %{prj.location}/shaders/script_compile.ps1 ./shaders/"
    }

project "Packager"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++23"
    targetdir "bin/%{cfg.buildcfg}"
    architecture("x86_64")
    filter "configurations:Clang"
        toolset("clang")
    filter {}
    debugdir "./Packager/"
    flags { "MultiProcessorCompile" }

    links { "fastgltf" }
    links { "nvtt30205" }

    includedirs { vk_sdk .. "/Include/" }
    includedirs { "libs/fastgltf/include/" }
    includedirs { "\"" .. nvtt_path .. "/include/\"" }
    includedirs { "libs/stb" }

    libdirs { "libs/fastgltf/build/Debug" }
    libdirs { "\"" .. nvtt_path .. "/lib/x64-v142/\"" }

    files { "libs/stb_image.cpp" }
    files { "Packager/**.h", "Packager/**.cpp" }
    files { "Engine/Types.h", "Engine/Telemetry.h", "Engine/Telemetry.cpp" }

    defines { "KHRONOS_STATIC" }
    filter { "configurations:Debug or configurations:RenderDoc or configurations:Clang" }
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

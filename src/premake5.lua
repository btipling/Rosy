workspace "Rosy"
    configurations { "Debug", "Release", "RenderDoc", "Clang" }
    filter "not configurations:Clang"
        warnings "Extra"
        fatalwarnings "All"
    filter {}

vk_sdk = os.getenv("VULKAN_SDK")

project "Engine"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    targetdir "bin/%{cfg.buildcfg}"
    architecture("x86_64")
    filter "configurations:Clang"
        toolset("clang")
    filter {}
    debugdir "./Engine/"
    flags { "MultiProcessorCompile" }

    links { "SDL3" }
    links { "flecs" }
    links { "ktx" }

    includedirs { "libs/SDL/include/" }
    includedirs { vk_sdk .. "/Include/" }
    includedirs { "libs/imgui/" }
    includedirs { "libs/tracy/" }
    includedirs { "libs/json/single_include/" }
    includedirs { "libs/KTX-Software/include/" }
    includedirs { "libs/flecs/include/" }

    libdirs { "libs/SDL/build/Debug" }
    libdirs { "libs/KTX-Software/build/Debug" }
    libdirs { vk_sdk .. "/Lib/" }
    libdirs { "libs/flecs/out/Debug" }
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
    cppdialect "C++20"
    targetdir "bin/%{cfg.buildcfg}"
    architecture("x86_64")
    filter "configurations:Clang"
        toolset("clang")
    filter {}
    debugdir "./Packager/"
    flags { "MultiProcessorCompile" }

    links { "fastgltf" }

    includedirs { "libs/fastgltf/include/" }

    libdirs { "libs/fastgltf/build/Debug" }

    files { "Packager/**.h", "Packager/**.cpp" }
    files { "Engine/Types.h", "Engine/Telemetry.h", "Engine/Telemetry.cpp" }

    filter { "configurations:Debug or configurations:RenderDoc or configurations:Clang" }
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

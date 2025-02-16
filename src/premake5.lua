workspace "Rosy"
   configurations { "Debug", "Release" }
   warnings "Extra"
   fatalwarnings "All"

vk_sdk = os.getenv("VULKAN_SDK")

project "Engine"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"
   targetdir "bin/%{cfg.buildcfg}"
   architecture("x86_64")
   -- toolset("clang")
   debugdir "./Engine/"
   flags { "MultiProcessorCompile" }

   links { "SDL3" }
   links { "flecs" }
   links { "ktx" }

   includedirs { "libs/SDL/include/" }
   includedirs { vk_sdk .. "/Include/" }
   includedirs { "libs/imgui/" }
   includedirs { "libs/tracy/" }
   includedirs { "libs/KTX-Software/include/" }
   includedirs { "libs/flecs/include/" }

   libdirs { "libs/SDL/build/Debug" }
   libdirs { "libs/KTX-Software/build/Debug" }
   libdirs { vk_sdk .. "/Lib/" }
   libdirs { "libs/flecs/out/Debug" }



   files { "Engine/**.h", "Engine/**.cpp" }
   files { "Packager/Asset.h", "Packager/Asset.cpp" }
   files { "libs/imgui/**.h", "libs/imgui/**.cpp" }
   files { "libs/Volk.cpp" }
   files { "libs/VMA.cpp" }
   files { "shaders/*.slang", "shaders/*.ps1" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

    buildmessage "Compiling shaders?"
    prebuildcommands {
        "/shaders/script_compile.ps1 ./shaders/"
    }

project "Packager"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"
   targetdir "bin/%{cfg.buildcfg}"
   architecture("x86_64")
   -- toolset("clang")
   debugdir "./Packager/"
   flags { "MultiProcessorCompile" }

   links { "fastgltf" }

   includedirs { "libs/fastgltf/include/" }

   libdirs { "libs/fastgltf/build/Debug" }


   files { "Packager/**.h", "Packager/**.cpp" }
   files { "Engine/Types.h", "Engine/Telemetry.h", "Engine/Telemetry.cpp" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

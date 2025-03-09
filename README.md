# Rosy

This is a hobby project to build a game engine and eventually a game in my spare time.

![image](https://github.com/user-attachments/assets/7202b428-e495-4829-a679-1d1473247b19)


![Rosy and friends](https://github.com/user-attachments/assets/1109c417-88a5-40d2-aa83-c1ff4296b977)

![Rosy and a shadow casting dragon](https://github.com/user-attachments/assets/92573443-0900-4f11-807f-ab386c76bb5b)

![cosTheta debug colors in sponza look cool](https://github.com/user-attachments/assets/cf4171fd-d414-4110-a729-c0ed0bfeac64)

![A beautiful game](https://github.com/user-attachments/assets/c49452dd-95f1-475c-95f3-2e5f0af3101f)

## Modern Vulkan

This project uses modern Vulkan features including:

* Buffer device address
* Dynamic rendering
* Bindless
* Shader Objects
* One global descriptor set for all images and samplers

## Building

### Git submodules

Git submodules are used for some dependencies, they are not built automatically. They must be built with the same version of msbuild.

```txt
git submodule init
git submodule update
```

### Premake

This project uses [Premake](https://premake.github.io/) to build. Premake is required to build the project.

```txt
cd ./src
premake5 vs2022
```

On Windows this will generate a .sln file that can be opened in Visual Studio 2022.

The premake5.lua script serves as the most up to date documentation for what is required to build this project. 

### Properietary Dependencies

The project is open source but it depends on non-free depdencies.

#### Vulkan SDK

The most recent version of the Vulkan SDK should be on the system. The Vulkan SDK can be downloaded from the [LunarG's Vulkan SDK website](https://www.lunarg.com/vulkan-sdk/).

#### FBX SDK

The most recent version of the FBX SDK should be on the system. The FBX SDK can be downloaded from the [Autodesk's FBX SDK website](https://aps.autodesk.com/developer/overview/fbx-sdk).

#### NVTT

Using the Packager program to create assets requires the NVIDIA Texture Tool library which defaults to CPU on non-NVIDIA hardware. The game itself uses this library to read DDS files and builds and should eventually be able to run fine on NVIDIA hardware I hope.
In order to use the Packager tool the `NVTT_PATH` env variable must be set to where the header files are and the shipped DLL must be in the same directory as Packager.exe. The
path is likely `C:\Program Files\NVIDIA Corporation\NVIDIA Texture Tools` on Windows. NVTT can be downloaded at the [NVIDIA Texture Tools 3 website](https://developer.nvidia.com/gpu-accelerated-texture-compression).

NVTT is used to compress and generate mipmaps for asset textures. It does so using a better API than libktx and uses available GPU hardware to compress images faster than libktx can. This makes a big difference when you are compressing many images as part of an asset pipeline. The compression speeds are multiple orders of magnitudes faster when using NVTT on an NVIDIA GPU. Every model ships with a slew of PBR textures, normal maps, albedo, metallic, etc and with libkts running on the CPU it can take tens of seconds to compress each large image. It adds up.

#### FastGLTF

```txt
cd .\libs\fastgltf\
cmake . -B build
```

Add fastgltf project to VS if it's not already there and build it

#### SDL

```txt
cd .\libs\SDL\
cmake . -B build
cmake --build build
```

#### flecs

```txt
cd .\libs\flecs\
cmake . -B out
cmake --build out
```

## Running

Windows requires the needed .dll files be in the same directory as the executable, this isn't done automatically.

Rosy has its own asset format, a gltf file can be converted to the .rsy format with the Packeer.exe which is built when the solution is built.

There are currently some hard coded asset paths in the level JSON file and in Editor.cpp that I need to clean up. The project will halt immediately if those assets are not there. They must be removed and replaced with other formats.

### Hardware

I have only tested this on Nvidia 3070 and 3060 gpus. I don't know if this application works on AMD or other GPUs at this time.


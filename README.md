# Rosy

This is a hobby project to build a game engine and eventually a game in my spare time.

![Rosy and friends](https://github.com/user-attachments/assets/234e05b4-a331-44f8-b6fd-01a31d1feac8)

![Sponza1](https://github.com/user-attachments/assets/253179b8-f070-4cc4-be9d-d5019d2a9408)

![Sponza2](https://github.com/user-attachments/assets/4718e857-89e4-46c0-95f2-5ec98cf3c346)

![A beautiful game](https://github.com/user-attachments/assets/33a61b06-374b-4efa-8f70-eee70e478b15)

## Modern Vulkan

This project uses modern Vulkan features including:

* Buffer device address
* Dynamic rendering
* Bindless
* Shader Objects
* One global descriptor set for all images and samplers

## Portability

Ideally this project would run cross platform, it uses SDL and Vulkan, but there are a couple of known issues:

* the use fopen_s in loading assets code.
* the use of "b" when using fopen_s as reading from binary files.

These likely do not work on non-Windows platforms.

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

### Proprietary Dependencies

The project is open source, but it depends on non-free dependencies.

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

Open fastgltf sln in VS and build it for debug and release.

#### SDL

```txt
cd .\libs\SDL\
cmake . -B build
cmake --build build
```

Open SDL sln in VS and build it for debug and release.

#### flecs

```txt
cd .\libs\flecs\
cmake . -B out
cmake --build out
```

Open flecs sln in VS and build it for debug and release.

#### meshoptimizer

```txt
cd .\libs\meshoptimizer\
cmake . -B out
cmake --build out
```

mesh optimizer needs to be built for release and debug in Visual Studio.

## Running

Windows requires the needed .dll files be in the same directory as the executable, this isn't done automatically.

Rosy has its own asset format. A glTF file can be converted to the .rsy format using Packager.exe, which is built when the solution is compiled.

Assuming there's an sponza.gltf on the system in an assets directory the packager can be run as so and it will add a sponza.rsy and generate *.dds images in the same directory as the sponza.gltf.

```txt
 .\bin\Debug\Packager.exe .\assets\sponza\sponza.gltf
 ```

There are currently some hard coded asset paths in the level JSON file and in Editor.cpp that I need to clean up. The project will halt immediately if those assets are not there. They must be removed and replaced with other rsy assets present on the system.

### Hardware

I have only tested this on Nvidia 3070 and 3060 gpus. I don't know if this application works on AMD or other GPUs at this time.


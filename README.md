# Rosy

This is a hobby project to build a game engine and eventually a game in my spare time.

![Sponza](https://github.com/user-attachments/assets/5cede515-33e1-488b-970f-f091bd9bc6ad)

![Rosy and friends](https://github.com/user-attachments/assets/1109c417-88a5-40d2-aa83-c1ff4296b977)

## Building

This project will likely not build for anyone else at this time. See assets and hardware section below.

### Premake

This project uses [Premake](https://premake.github.io/) to build. Premake is required to build the project.

I initialize the project by running `premake5 vs2022` on the command line in the `src` directory and then I open up the generated sln file in VIsual Studio.

### Vulkan SDK

The most recent version of the Vulkan SDK should be on the system. The Vulkan SDK can be downloaded from the [LunarG's Vulkan SDK website](https://www.lunarg.com/vulkan-sdk/).

### NVTT

Using the Packager program to create assets requires the NVIDIA Texture Tool library which defaults to CPU on non-NVIDIA hardware. The game itself uses this library to read DDS files and builds and should eventually be able to run fine on NVIDIA hardware I hope.
In order to use the Packager tool the `NVTT_PATH` env variable must be set to where the header files are and the shipped DLL must be in the same directory as Packager.exe. The
path is likely `C:\Program Files\NVIDIA Corporation\NVIDIA Texture Tools` on Windows. NVTT can be downloaded at the [NVIDIA Texture Tools 3 website](https://developer.nvidia.com/gpu-accelerated-texture-compression).

NVTT is used to compress and generate mipmaps for asset textures. It does so using a better API than libktx and uses available GPU hardware to compress images faster than libktx can. This makes a big difference when you are compressing many images as part of an asset pipeline. The compression speeds are multiple orders of magnitudes faster when using NVTT on an NVIDIA GPU. Every model ships with a slew of PBR textures, normal maps, albedo, metallic, etc and with libkts running on the CPU it can take tens of seconds to compress each large image. It adds up.

### Git submodules

KTX, SDL3, Fastgltf and flecs are now gitsubmodules
SDL3 and flecs are dynamically linked, build the dlls and include next to binary, add lib files after building

```txt
git submodule init
git submodule update
```

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

### Assets

Assets are not included in the repository and the application will immediately halt without them. I don't currently have a good solution
for distributing assets, it's all very manual. I could zip my asset directory on request.

### Hardware

I have only tested this on Nvidia 3070 and 3060 gpus. I don't know if this application works on AMD or other GPUs at this time.

## Modern Vulkan

* Buffer device address
* Dynamic rendering
* Bindless
* Shader Objects
* One global descriptor set for all images and samplers

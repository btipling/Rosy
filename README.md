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

The most recent version of the Vulkan SDK should be on the system.

### Git submodules

KTX, SDL3, Fastgltf and flecs are now gitsubmodules
SDL3 and flecs are dynamically linked, build the dlls and include next to binary, add lib files after building

```txt
git submodule init
git submodule update
```

#### KTX

```txt
cd .\libs\KTX-Software\
cmake . -B build
cmake --build build
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

## TODO

* [ ] config for physical device to use
* [ ] read links in <https://github.com/Darianopolis/Links/blob/main/Links.txt>

## Generating Cubemaps with ktx

```txt
ktx create --format R8G8B8A8_SRGB --generate-mipmap --mipmap-filter box --encode uastc --uastc-quality 0  --zstd 5 --cubemap  .\xp.png .\xn.png .\yp.png .\yn.png .\zp.png .\zn.png  skybox.ktx2
 ```

 Then have to open them up in Nvidia Texture Tools and save them to get the VkFormat correct

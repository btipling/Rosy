# Rosy
This is a hobby project to build a game engine and eventually a game in my spare time.

![Sponza](https://github.com/user-attachments/assets/5cede515-33e1-488b-970f-f091bd9bc6ad)

## Building
This project will likely not build for anyone else at this time.

* libs is where the .lib files should go for third party dependencies
* src/x64/Debug is where .dll files for third party dependencies should go
* SDL_PATH, KTX_PATH etc describe required environment variables.
* [SDL3](https://github.com/libsdl-org/SDL) dll and lib needed
    * SDL_PATH must be set to where SDL3's git repo is to find the include headers folder
    * SDL must be built and the SDL.dll and SDL.lib files must be added to the project
* [KTX](https://github.com/KhronosGroup/KTX-Software/tree/main)
    * KTX_PATH needs to be set and built and the dll and lib needed to be added.
* The Vulkan SDK must be installed and the VULKAN_SDK environment variable must be set.
* [fastgltf](https://github.com/spnda/fastgltf) must be [statically linked](https://learn.microsoft.com/en-us/cpp/build/walkthrough-creating-and-using-a-static-library-cpp?view=msvc-170#UseLibInApp)
    * `cd src/libs`
    * `git clone git@github.com:spnda/fastgltf.git`
    * `cd fastgltf`
    * `cmake -B build`
    * Add .\src\libs\fastgltf\build\fastgltf.vcxproj as a project on Engine 
    * Add a reference to the fastgltf project on Packager
    * Both the Packager and Engine need the src/libs/fastgltf/include included in Additional include directories in project settings
    * Hopefull the last three steps just already work since it's committed to the project and solution files.
* TODO: Use git submodules for all of this.

Assets are not included in the repository and the application will immediately halt without them.

This application is hard coded to run on an Nvidia GPU and Windows 11.

## Modern Vulkan 
* Buffer device address
* Dynamic rendering
* Bindless
* Shader Objects
* One global descriptor set for all images and samplers

## TODO
* [ ] config for physical device to use
* [ ] read links in https://github.com/Darianopolis/Links/blob/main/Links.txt

## Generating Cubemaps with ktx

```
 ktx create --format R8G8B8A8_SRGB --generate-mipmap --mipmap-filter box --encode uastc --uastc-quality 0  --zstd 5 --cubemap  .\xp.png .\xn.png .\yp.png .\yn.png .\zp.png .\zn.png  skybox.ktx2
 ```
 Then have to open them up in Nvidia Texture Tools and save them to get the VkFormat correct

 ## Open Source Code Dependencies

### Vulkan Tools and the Vulkan SDK
https://github.com/LunarG/VulkanTools<br/>
Apache License  2.0, January 2004<br/>

 ### KTX tools
 https://github.com/KhronosGroup/KTX-Software   <br/>
 Apache 2.0 license <br/>
 Copyright (c) Mark Callow, the KTX-Software author; The Khronos Group Inc. <br/>
 and additional licenses<br/>
 https://github.com/KhronosGroup/KTX-Software/tree/main/LICENSES

 ### fastgltf 
 https://github.com/spnda/fastgltf  <br/>
 MIT license <br/>
 Copyright (c) 2022 - 2024 spnda.  <br/>

 ### Vulkan memory allocator
 https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator  <br/>
 MIT License  <br/>
 Copyright  (c) 2017-2024 Advanced Micro Devices, Inc. <br/>

 ### Dear Imgui 
 https://github.com/ocornut/imgui  <br/>
 MIT license  <br/>
 Copyright (c) 2014-2024 Omar Cornut <br/>

 ### Volk
 https://github.com/zeux/volk  <br/>
 MIT license  <br/>
 Copyright (c) 2018-2024 Arseny Kapoulkine <br/>


 ### DirectXTex 
 https://github.com/microsoft/DirectXTex  <br/>
 MIT license  <br/>
 Copyright (c) Microsoft Corporation.
  <br/>

 ### stb
 https://github.com/nothings/stb/tree/master?tab=License-1-ov-file#readme   <br/>
 MIT License  <br/>
 Copyright (c) 2017 Sean Barrett <br/>

 ### VkGuide
 https://github.com/vblanco20-1/vulkan-guide <br/>
 MIT License<br/>
 Copyright (c) 2022-2024 2016 VkGuide Author<br/>
 I initially relied a lot on this for developing my scene graph.<br/>

 ### Nameless engine
 https://github.com/Flone-dnb/nameless-engine <br/>
 MIT License<br/>
 Copyright (c) 2022-2024 Alexander Tretyakov<br/>
 For some of the ktx vma allocation logic.<br/>

[skybox attribution](https://sketchfab.com/3d-models/free-skybox-blue-desert-fd952e60be9746e0872840e89fbf7370)

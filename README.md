# Rosy

## Building
This hobby project won't build for anyone else but me and my PC and laptop.

* [SDL3](https://github.com/libsdl-org/SDL) dll and lib needed
* [KTX](https://github.com/KhronosGroup/KTX-Software/tree/main) dll and lib needed
* [vcpkg](https://vcpkg.io/en/) and vcpkg install needed

Assets are not included in the repository and the application will immediately halt without them.

This application is hard coded to run on an Nvidia GPU and Windows 11.

## TODO
* [ ] config for physical device to use
* [ ] bindless resources https://henriquegois.dev/posts/bindless-resources-in-vulkan/
* [ ] more bindless https://blog.traverseresearch.nl/bindless-rendering-setup-afeb678d77fc?gi=ff07f56c3097
* [ ] descriptor indexing https://chunkstories.xyz/blog/a-note-on-descriptor-indexing/
* [ ] resource sync https://themaister.net/blog/2019/08/14/yet-another-blog-explaining-vulkan-synchronization/
* [ ] read links in https://github.com/Darianopolis/Links/blob/main/Links.txt
* [x] Do resize and draw in SDL resize callback for maximum responsiveness

![image](https://github.com/user-attachments/assets/258f0c51-2988-4b21-98f6-46773aacacd0)

![image](https://github.com/user-attachments/assets/a1b25224-a83d-4c76-ae80-d58c86cdf140)

## Generating Cubemaps with ktx

```
 ktx create --format R8G8B8A8_SRGB --generate-mipmap --mipmap-filter box --encode uastc --uastc-quality 0  --zstd 5 --cubemap  .\xp.png .\xn.png .\yp.png .\yn.png .\zp.png .\zn.png  skybox.ktx2
 ```
 Then have to open them up in Nvidia Texture Tools and save them to get the VkFormat correct

 ## Open Source Code Dependencies

 ### Nameless engine

 https://github.com/Flone-dnb/nameless-engine MIT License Copyright (c) 2022-2024 Alexander Tretyakov
 For some of the ktx vma allocation logic.

 ### KTX tools

 https://github.com/KhronosGroup/KTX-Software  Apache 2.0 license Copyright (c) Mark Callow, the KTX-Software author; The Khronos Group Inc. 
 and additional licenses
 https://github.com/KhronosGroup/KTX-Software/tree/main/LICENSES

 ### fastgltf 

 https://github.com/spnda/fastgltf MIT license Copyright (c) 2022 - 2024 spnda. All rights reserved.

 ### Vulkan memory allocator

 https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator MIT License Copyright (c) 2017-2024 Advanced Micro Devices, Inc. All rights reserved.

 ### Dear Imgui 

 https://github.com/ocornut/imgui MIT license Copyright (c) 2014-2024 Omar Cornut

 ### Volk

 https://github.com/zeux/volk MIT license Copyright (c) 2018-2024 Arseny Kapoulkine


 ### DirectXTex 

 https://github.com/microsoft/DirectXTex MIT license Copyright (c) Microsoft Corporation.

 ### stb

 https://github.com/nothings/stb/tree/master?tab=License-1-ov-file#readme  MIT License Copyright (c) 2017 Sean Barrett
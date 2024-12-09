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
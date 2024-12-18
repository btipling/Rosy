
if (-not (Test-Path -Path "out")) {
    New-Item -ItemType Directory -Path "out" | Out-Null
}
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/mesh.vert -o out/mesh.vert.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/mesh.frag -o out/mesh.frag.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/skybox_mesh.frag -o out/skybox_mesh.frag.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/skybox.vert -o out/skybox.vert.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/skybox.frag -o out/skybox.frag.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/shadow.vert -o out/shadow.vert.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/shadow.frag -o out/shadow.frag.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/debug.vert -o out/debug.vert.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/debug.frag -o out/debug.frag.spv
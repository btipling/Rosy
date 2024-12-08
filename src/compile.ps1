
if (-not (Test-Path -Path "out")) {
    New-Item -ItemType Directory -Path "out" | Out-Null
}
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/mesh.vert -o out/mesh.vert.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/mesh.frag -o out/mesh.frag.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/skybox.vert -o out/skybox.vert.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/skybox.frag -o out/skybox.frag.spv

if (-not (Test-Path -Path "out")) {
    New-Item -ItemType Directory -Path "out" | Out-Null
}
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/skybox_mesh.frag -o out/skybox_mesh.frag.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/skybox.vert -o out/skybox.vert.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/skybox.frag -o out/skybox.frag.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/shadow.vert -o out/shadow.vert.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/shadow.frag -o out/shadow.frag.spv
& "slangc.exe" shaders/mesh.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/mesh.spv 
& "slangc.exe" shaders/debug.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/debug.spv 
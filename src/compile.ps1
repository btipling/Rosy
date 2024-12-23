
if (-not (Test-Path -Path "out")) {
    New-Item -ItemType Directory -Path "out" | Out-Null
}
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/skybox.vert -o out/skybox.vert.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/skybox.frag -o out/skybox.frag.spv
& "slangc.exe" shaders/shadow.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/shadow.spv 
& "slangc.exe" shaders/skybox.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/skybox.spv 
& "slangc.exe" shaders/mesh.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/mesh.spv 
& "slangc.exe" shaders/debug.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/debug.spv 
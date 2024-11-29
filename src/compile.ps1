
if (-not (Test-Path -Path "out")) {
    New-Item -ItemType Directory -Path "out" | Out-Null
}
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/shader.vert -o out/vert.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/shader.frag -o out/frag.spv
& "$env:VULKAN_SDK\Bin\glslc.exe" shaders/tex_image.frag -o out/tex_image.frag.spv
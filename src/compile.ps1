
if (-not (Test-Path -Path "out")) {
    New-Item -ItemType Directory -Path "out" | Out-Null
}
& "slangc.exe" $PSScriptRoot/shaders/skybox_cube.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/skybox_cube.spv 
& "slangc.exe" $PSScriptRoot/shaders/shadow.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/shadow.spv 
& "slangc.exe" $PSScriptRoot/shaders/skybox.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/skybox.spv 
& "slangc.exe" $PSScriptRoot/shaders/mesh.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/mesh.spv 
& "slangc.exe" $PSScriptRoot/shaders/debug.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/debug.spv 
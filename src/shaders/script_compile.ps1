
if (-not (Test-Path -Path "out")) {
    New-Item -ItemType Directory -Path "out" | Out-Null
}
& "slangc.exe" $PSScriptRoot/skybox_cube.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/skybox_cube.spv 
& "slangc.exe" $PSScriptRoot/shadow.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/shadow.spv 
& "slangc.exe" $PSScriptRoot/skybox.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/skybox.spv 
& "slangc.exe" $PSScriptRoot/mesh.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/mesh.spv 
& "slangc.exe" $PSScriptRoot/debug.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/debug.spv 
& "slangc.exe" $PSScriptRoot/basic.slang -matrix-layout-column-major -profile sm_6_6 -target spirv -o out/basic.spv 
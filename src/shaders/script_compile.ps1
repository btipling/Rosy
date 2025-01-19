$slangc = Get-Command "slangc.exe" -ErrorAction SilentlyContinue
if (-not $slangc) {
    Write-Error "slangc.exe not found in PATH"
    exit 1
}

if (-not (Test-Path -Path "out")) {
    New-Item -ItemType Directory -Path "out" | Out-Null
}

$shaders = @(
    "skybox_cube",
    "shadow",
    "skybox",
    "mesh",
    "debug",
    "basic"
)

$shaders | ForEach-Object {
    $shader = $_
    $result = & $slangc $PSScriptRoot/$shader.slang `
        -matrix-layout-column-major `
        -profile sm_6_6 `
        -target spirv `
        -o out/$shader.spv 2>&1
    Write-Host ($result | Format-Table | Out-String)
    if ($LASTEXITCODE -ne 0) {
            Write-Host "Failed to compile $shader.slang"
            exit 1
    }
    Write-Host "Compiled $shader.slang successfully"
        
}
Write-Host "Compiled all shaders successfully"
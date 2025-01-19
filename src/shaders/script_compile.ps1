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

$jobs = $shaders | ForEach-Object {
    $shader = $_
    
    Start-Job -ScriptBlock {
        param($slangc, $scriptRoot, $shader)
        $result = & $slangc $scriptRoot/$shader.slang `
            -matrix-layout-column-major `
            -profile sm_6_6 `
            -target spirv `
            -o out/$shader.spv 
        if ($LASTEXITCODE -ne 0) {
                Write-Error "Failed to compile $shader.slang"
                exit 1
        }
        Write-Host "Compiled $shader.slang successfully"
        
    } -ArgumentList $slangc.Source, $PSScriptRoot, $shader
}

$jobs | Wait-Job | Receive-Job
if ($LASTEXITCODE -ne 0) {
    exit 1
}

Write-Host "Compiled all shaders successfully"
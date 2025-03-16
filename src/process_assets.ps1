# Check if a level file was provided as an argument
param(
    [Parameter(Mandatory = $true)]
    [string]$levelFile
)

$currentDir = Get-Location
Write-Host "Current directory: $currentDir"

# Verify the level file exists
if (-not (Test-Path $levelFile)) {
    Write-Error "Level file not found: $levelFile"
    exit 1
}

# Read and parse the JSON file
$levelData = Get-Content $levelFile -Raw | ConvertFrom-Json

# Get the directory of the level file to resolve relative paths
$levelDir = Split-Path -Parent $levelFile

# Process each asset
foreach ($asset in $levelData.assets) {
    # Convert the relative path to absolute path, but replace .rsy with potential source extensions
    # The asset paths are assumed to be relative to the Engine directory
    $rsyPath = $currentDir.Path + "\Engine\" + $asset.Path

    # Get the directory and filename without extension
    $assetDir = Split-Path -Parent $rsyPath
    $assetName = [System.IO.Path]::GetFileNameWithoutExtension($rsyPath)

    Write-Host "Asset directory: $assetDir"

    # Look for source files
    $fbxPath = Join-Path $assetDir "$assetName.fbx"
    $gltfPath = Join-Path $assetDir "$assetName.gltf"

    $sourcePath = $null
    Write-Host "FBX path: $fbxPath"
    Write-Host "GLTF path: $gltfPath"
    if (Test-Path $fbxPath) {
        $sourcePath = $fbxPath
    }
    elseif (Test-Path $gltfPath) {
        $sourcePath = $gltfPath
    }

    if ($null -eq $sourcePath) {
        Write-Warning "No source file (fbx/gltf) found for asset: $assetName in directory: $assetDir"
        continue
    }

    # Run the packager on the source asset
    Write-Host "Processing asset: $sourcePath"
    & ".\bin\Debug\Packager.exe" $sourcePath

    # Check if the packager succeeded
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Packager failed for asset: $sourcePath"
    }
}

Write-Host "Asset processing complete!"

[hashtable]$empty_dependencies = [ordered]@{
    dependencies = @(  )
}
$empty_deps_string = $empty_dependencies | ConvertTo-Json

[hashtable]$dependencies = [ordered]@{
    dependencies = @(
        'directxtex',
        'fastgltf',
        # 'tracy',
        'stb'
    )
}
$deps_string = $dependencies | ConvertTo-Json

Write-Host "Removing deps"
Set-Content -Path .\vcpkg.json -Value $empty_deps_string
vcpkg install
Write-Host "Reinstalling deps"
Set-Content -Path .\vcpkg.json -Value $deps_string
vcpkg install

function Watch-File {
    [cmdletbinding()]
    Param (
        [string]
        $Path
    )
    $fileWatcher = New-Object System.IO.FileSystemWatcher
    $fileWatcher.Path = $Path
    $fileWatcher.Filter = "*.*"
    $fileWatcher.IncludeSubdirectories = $true
    $fileWatcher.EnableRaisingEvents = $true

    $action = {
        $start = Get-Date
        $message = "Building shaders."
        Write-Host $message 
        &.\compile.ps1
        $done = Get-Date
        $res = $done - $start
        $end_message = "Finished in '$res.TotalSeconds"
        Write-Host $end_message 
    }

    Register-ObjectEvent -InputObject $fileWatcher -EventName "Changed" -Action $action

Write-Host "Monitoring shaders for changes at: $Path. Press Ctrl+C to stop."

    try {
        while ($true) {
            Start-Sleep -Seconds 1
        }
    }
    finally {
        Unregister-Event -SourceIdentifier FileChanged
        $fileWatcher.EnableRaisingEvents = $false
    }
}

Watch-File $PSScriptRoot\shaders
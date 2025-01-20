
$global:should_quit = $false

function Watch-File {
    [cmdletbinding()]
    Param (
        [string]
        $Path
    )
    $fileWatcher = New-Object System.IO.FileSystemWatcher
    $fileWatcher.Path = $Path
    $fileWatcher.Filter = "*.*"
    $fileWatcher.IncludeSubdirectories = $false
    $fileWatcher.EnableRaisingEvents = $true
    Write-Host "Watching $fileWatcher.Path"
    $action = {
        Write-Host ($Event | Format-Table | Out-String)
        Write-Host ($Event | Format-List | Out-String)
        Write-Host ($EventArgs | Format-Table | Out-String)
        Write-Host ($EventArgs | Format-List | Out-String)
        $start = Get-Date
        $message = "Building shaders."
        Write-Host $message 
        try {
            $output = &.\script_compile.ps1
            if ($LASTEXITCODE -ne 0) {
                Write-Host "Shader compilation failed; $output"
            }
        }
        catch {
            Write-Error "Failed to execute shader compilation: $_"
        }
        $done = Get-Date
        $res = $done - $start
        $Error | Get-Error
        $end_message = "Finished in $($res.TotalSeconds) seconds"
        Write-Host $end_message 
    }

$eventJob = Register-ObjectEvent -InputObject $fileWatcher -EventName "Changed" -Action $action

Write-Host "Monitoring shaders for changes at: $Path. Press Ctrl+C to stop."

    try {
        while ($true) {
            Start-Sleep -Seconds 1
            if ($global:should_quit) {
                Write-Host "quitting"
                exit 0
            } 
        }
    }
    finally {
        if ($eventJob) {
             Unregister-Event -SourceIdentifier $eventJob.Name
             Remove-Job -Job $eventJob -Force
        }
        $fileWatcher.EnableRaisingEvents = $false
    }
}
Watch-File $PSScriptRoot
param([string]$UV4 = 'C:\Keil_v5\UV4\UV4.exe')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$mdkDirectory = Join-Path $repoRoot 'MDK-ARM'
$originalProject = Join-Path $mdkDirectory 'smart_door.uvprojx'
$checkProject = Join-Path $mdkDirectory 'dma_check.uvprojx'
$buildLog = Join-Path $mdkDirectory 'dma_check.log'
if (-not (Test-Path -LiteralPath $UV4)) { throw "Keil not found: $UV4" }
# Keep the generated project beside the original so all source/include paths resolve.
# Original project settings and existing smart_door build outputs remain untouched.
$projectText = Get-Content -LiteralPath $originalProject -Raw
$projectText = [regex]::Replace($projectText, '<OutputDirectory>.*?</OutputDirectory>', '<OutputDirectory>dma_check\</OutputDirectory>')
$projectText = [regex]::Replace($projectText, '<ListingPath>.*?</ListingPath>', '<ListingPath>dma_check\</ListingPath>')
$projectText = [regex]::Replace($projectText, '<RunUserProg([12])>1</RunUserProg[12]>', '<RunUserProg$1>0</RunUserProg$1>')
[System.IO.File]::WriteAllText($checkProject, $projectText, (New-Object System.Text.UTF8Encoding($false)))
$process = Start-Process -FilePath $UV4 -ArgumentList '-b dma_check.uvprojx -t smart_door -o dma_check.log' -WorkingDirectory $mdkDirectory -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(60000)) {
    $process.Kill()
    throw 'Keil did not complete within 60 seconds. Check the isolated project and compiler installation.'
}
Get-Content -LiteralPath $buildLog
if ($process.ExitCode -ge 2) { throw "Keil build failed: exit code $($process.ExitCode)" }
Write-Output "HEX: $(Join-Path $mdkDirectory 'dma_check/smart_door.hex')"

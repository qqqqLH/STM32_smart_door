param([string]$Compiler = 'gcc')
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    New-Item -ItemType Directory -Force 'tests/build' | Out-Null
    foreach ($testName in @('finger_dma', 'oled_dma', 'voice')) {
        $outputPath = "tests/build/test_${testName}.exe"
        $compileArgs = @('-std=c99', '-Wall', '-Wextra', '-Werror', '-fanalyzer',
            '-Wno-pointer-to-int-cast', '-Wno-missing-braces', '-Itests/stubs',
            '-IBSP/FINGER', '-IBSP/OLED', "tests/test_${testName}.c", '-o', $outputPath)
        & $Compiler @compileArgs
        if ($LASTEXITCODE -ne 0) { throw "Failed to compile $testName tests." }
        & "./$outputPath"
        if ($LASTEXITCODE -ne 0) { throw "$testName tests failed." }
    }
} finally { Pop-Location }

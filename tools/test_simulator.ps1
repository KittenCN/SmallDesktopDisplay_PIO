[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',

    [string]$BuildDirectory = '',

    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Find-CMakeExecutable {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    $vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) `
        'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $installationPath = & $vswhere `
            -latest `
            -products '*' `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath |
            Select-Object -First 1
        if (-not [string]::IsNullOrWhiteSpace($installationPath)) {
            $bundledCMake = Join-Path $installationPath.Trim() `
                'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path -LiteralPath $bundledCMake -PathType Leaf) {
                return $bundledCMake
            }
        }
    }

    throw 'Unable to find CMake. Run tools\build_simulator.ps1 for installation guidance.'
}

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repositoryRoot 'build\simulator'
}
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build_simulator.ps1') `
        -Configuration $Configuration `
        -BuildDirectory $BuildDirectory
    if ($LASTEXITCODE -ne 0) {
        throw "Simulator build failed with exit code $LASTEXITCODE"
    }
}

$cmake = Find-CMakeExecutable
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
if (-not (Test-Path -LiteralPath $ctest -PathType Leaf)) {
    $ctestCommand = Get-Command ctest.exe -ErrorAction SilentlyContinue
    if ($null -eq $ctestCommand) {
        throw "CTest was not found beside CMake or on PATH"
    }
    $ctest = $ctestCommand.Source
}

$previousVideoDriver = [Environment]::GetEnvironmentVariable(
    'SDL_VIDEODRIVER',
    'Process'
)
$previousRenderDriver = [Environment]::GetEnvironmentVariable(
    'SDL_RENDER_DRIVER',
    'Process'
)

try {
    $env:SDL_VIDEODRIVER = 'dummy'
    $env:SDL_RENDER_DRIVER = 'software'

    & $ctest `
        --test-dir $BuildDirectory `
        -C $Configuration `
        --output-on-failure `
        --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "Simulator tests failed with exit code $LASTEXITCODE"
    }
}
finally {
    [Environment]::SetEnvironmentVariable(
        'SDL_VIDEODRIVER',
        $previousVideoDriver,
        'Process'
    )
    [Environment]::SetEnvironmentVariable(
        'SDL_RENDER_DRIVER',
        $previousRenderDriver,
        'Process'
    )
}

$executable = Join-Path $BuildDirectory `
    "bin\$Configuration\SmallDesktopDisplaySimulator.exe"
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "The simulator executable is missing: $executable"
}

$selfTestProcess = Start-Process `
    -FilePath $executable `
    -ArgumentList '--self-test' `
    -Wait `
    -PassThru
if ($selfTestProcess.ExitCode -ne 0) {
    throw "Simulator self-test failed with exit code $($selfTestProcess.ExitCode)"
}

$testOutputDirectory = Join-Path $BuildDirectory 'test-output'
New-Item -ItemType Directory -Path $testOutputDirectory -Force | Out-Null
$screenshot = Join-Path $testOutputDirectory 'headless-smoke.bmp'
if (Test-Path -LiteralPath $screenshot) {
    Remove-Item -LiteralPath $screenshot -Force
}
$headlessProcess = Start-Process `
    -FilePath $executable `
    -ArgumentList @('--headless', '--frames', '10', '--screenshot', "`"$screenshot`"") `
    -Wait `
    -PassThru
if ($headlessProcess.ExitCode -ne 0) {
    throw "Simulator headless smoke test failed with exit code $($headlessProcess.ExitCode)"
}
if (-not (Test-Path -LiteralPath $screenshot -PathType Leaf)) {
    throw "Simulator headless smoke test did not create a screenshot: $screenshot"
}
$screenshotInfo = Get-Item -LiteralPath $screenshot
if ($screenshotInfo.Length -le 54) {
    throw "Simulator headless smoke screenshot is unexpectedly small: $($screenshotInfo.Length) bytes"
}
$signature = [IO.File]::ReadAllBytes($screenshot)[0..1]
if ($signature[0] -ne 0x42 -or $signature[1] -ne 0x4D) {
    throw 'Simulator headless smoke screenshot is not a BMP file.'
}

Write-Host 'Simulator tests passed.'

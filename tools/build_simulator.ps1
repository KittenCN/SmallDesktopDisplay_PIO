[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',

    [string]$BuildDirectory = '',

    [switch]$Clean,
    [switch]$Run,
    [switch]$Package
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Find-VisualStudioInstallation {
    $programFilesX86 = [Environment]::GetFolderPath('ProgramFilesX86')
    $vswhereCandidates = @(
        (Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'),
        (Join-Path $PSScriptRoot 'vswhere.exe')
    )

    foreach ($vswhere in $vswhereCandidates) {
        if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
            continue
        }

        $installationPath = & $vswhere `
            -latest `
            -products '*' `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath |
            Select-Object -First 1

        if ([string]::IsNullOrWhiteSpace($installationPath)) {
            continue
        }

        return $installationPath.Trim()
    }

    throw @'
The MSVC x64 toolchain was not found.
Install Visual Studio Build Tools with the "Desktop development with C++"
workload, including CMake tools for Windows, then run this script again.
'@
}

function Find-CMakeExecutable {
    param([Parameter(Mandatory)] [string]$VisualStudioInstallation)

    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    $bundledCMake = Join-Path $VisualStudioInstallation `
        'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (Test-Path -LiteralPath $bundledCMake -PathType Leaf) {
        return $bundledCMake
    }

    throw 'CMake was not found on PATH or in the selected Visual Studio installation.'
}

function Find-NinjaExecutable {
    param([Parameter(Mandatory)] [string]$VisualStudioInstallation)

    $command = Get-Command ninja.exe -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    $bundledNinja = Join-Path $VisualStudioInstallation `
        'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
    if (Test-Path -LiteralPath $bundledNinja -PathType Leaf) {
        return $bundledNinja
    }

    throw 'Ninja was not found on PATH or in the selected Visual Studio installation.'
}

function Import-VisualStudioEnvironment {
    param([Parameter(Mandatory)] [string]$VisualStudioInstallation)

    $developerCommand = Join-Path $VisualStudioInstallation `
        'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path -LiteralPath $developerCommand -PathType Leaf)) {
        throw "Visual Studio developer command file was not found: $developerCommand"
    }

    $commandLine = "`"$developerCommand`" -no_logo -arch=x64 -host_arch=x64 >nul && set"
    $environmentLines = & $env:ComSpec /d /s /c $commandLine
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to initialize the Visual Studio x64 build environment."
    }

    foreach ($line in $environmentLines) {
        $separator = $line.IndexOf('=')
        if ($separator -le 0) {
            continue
        }

        $name = $line.Substring(0, $separator)
        $value = $line.Substring($separator + 1)
        [Environment]::SetEnvironmentVariable($name, $value, 'Process')
    }
}

function Invoke-CheckedNativeCommand {
    param(
        [Parameter(Mandatory)] [string]$FilePath,
        [Parameter(Mandatory)] [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath $($Arguments -join ' ')"
    }
}

function Assert-SafeBuildDirectory {
    param(
        [Parameter(Mandatory)] [string]$RepositoryRoot,
        [Parameter(Mandatory)] [string]$Candidate
    )

    $allowedRoot = [IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'build'))
    $resolvedCandidate = [IO.Path]::GetFullPath($Candidate)
    $allowedPrefix = $allowedRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar

    if (-not $resolvedCandidate.StartsWith(
            $allowedPrefix,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing destructive cleanup outside the repository build directory: $resolvedCandidate"
    }
}

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$sourceDirectory = Join-Path $repositoryRoot 'simulator'
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repositoryRoot 'build\simulator'
}
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)

if ($Clean -and (Test-Path -LiteralPath $BuildDirectory)) {
    Assert-SafeBuildDirectory -RepositoryRoot $repositoryRoot -Candidate $BuildDirectory
    Remove-Item -LiteralPath $BuildDirectory -Recurse -Force
}

$visualStudio = Find-VisualStudioInstallation
Import-VisualStudioEnvironment -VisualStudioInstallation $visualStudio
$cmake = Find-CMakeExecutable -VisualStudioInstallation $visualStudio
$ninja = Find-NinjaExecutable -VisualStudioInstallation $visualStudio
Write-Host "Using CMake: $cmake"
Write-Host "Using MSVC from: $visualStudio"
Write-Host "Using Ninja: $ninja"
Write-Host "Simulator build directory: $BuildDirectory"

Invoke-CheckedNativeCommand -FilePath $cmake -Arguments @(
    '-S', $sourceDirectory,
    '-B', $BuildDirectory,
    '-G', 'Ninja',
    "-DCMAKE_MAKE_PROGRAM=$ninja",
    "-DCMAKE_BUILD_TYPE=$Configuration",
    '-DBUILD_TESTING=ON'
)

Invoke-CheckedNativeCommand -FilePath $cmake -Arguments @(
    '--build', $BuildDirectory,
    '--config', $Configuration,
    '--parallel'
)

$executable = Join-Path $BuildDirectory `
    "bin\$Configuration\SmallDesktopDisplaySimulator.exe"
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "The simulator executable was not produced at the expected path: $executable"
}

if ($Package) {
    $packageScript = Join-Path $PSScriptRoot 'package_simulator.ps1'
    & $packageScript `
        -Configuration $Configuration `
        -BuildDirectory $BuildDirectory `
        -SkipBuild
    if ($LASTEXITCODE -ne 0) {
        throw "Simulator packaging failed with exit code $LASTEXITCODE"
    }
}

if ($Run) {
    Write-Host "Starting simulator: $executable"
    $simulatorProcess = Start-Process `
        -FilePath $executable `
        -WorkingDirectory (Split-Path -Parent $executable) `
        -PassThru `
        -Wait
    if ($simulatorProcess.ExitCode -ne 0) {
        throw "Simulator exited with code $($simulatorProcess.ExitCode)"
    }
}

Write-Host "Simulator build completed: $executable"

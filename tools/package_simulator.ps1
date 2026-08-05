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

function Assert-SafeStagingDirectory {
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
        throw "Refusing staging cleanup outside the repository build directory: $resolvedCandidate"
    }
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
$cpack = Join-Path (Split-Path -Parent $cmake) 'cpack.exe'
if (-not (Test-Path -LiteralPath $cpack -PathType Leaf)) {
    $cpackCommand = Get-Command cpack.exe -ErrorAction SilentlyContinue
    if ($null -eq $cpackCommand) {
        throw 'CPack was not found beside CMake or on PATH.'
    }
    $cpack = $cpackCommand.Source
}

$stagingDirectory = Join-Path $repositoryRoot "build\package\$Configuration"
$packagesDirectory = Join-Path $repositoryRoot 'build\packages'
Assert-SafeStagingDirectory `
    -RepositoryRoot $repositoryRoot `
    -Candidate $stagingDirectory

if (Test-Path -LiteralPath $stagingDirectory) {
    Remove-Item -LiteralPath $stagingDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $stagingDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $packagesDirectory -Force | Out-Null

& $cmake `
    --install $BuildDirectory `
    --config $Configuration `
    --prefix $stagingDirectory
if ($LASTEXITCODE -ne 0) {
    throw "Simulator install staging failed with exit code $LASTEXITCODE"
}

$requiredFiles = @(
    'SmallDesktopDisplaySimulator.exe',
    'SDL3.dll',
    'README.md',
    'THIRD_PARTY_NOTICES.txt',
    'LICENSE'
)
foreach ($fileName in $requiredFiles) {
    $filePath = Join-Path $stagingDirectory $fileName
    if (-not (Test-Path -LiteralPath $filePath -PathType Leaf)) {
        throw "The portable package is missing required file: $filePath"
    }
}

$cpackConfig = Join-Path $BuildDirectory 'CPackConfig.cmake'
if (-not (Test-Path -LiteralPath $cpackConfig -PathType Leaf)) {
    throw "CPack configuration is missing: $cpackConfig"
}

& $cpack `
    --config $cpackConfig `
    -C $Configuration `
    -G ZIP `
    -B $packagesDirectory
if ($LASTEXITCODE -ne 0) {
    throw "Simulator ZIP packaging failed with exit code $LASTEXITCODE"
}

$packageNameLine = Select-String `
    -LiteralPath $cpackConfig `
    -Pattern '^set\(CPACK_PACKAGE_FILE_NAME "([^"]+)"\)$' |
    Select-Object -First 1
if ($null -eq $packageNameLine) {
    throw "Unable to read CPACK_PACKAGE_FILE_NAME from: $cpackConfig"
}
$packageFileName = $packageNameLine.Matches[0].Groups[1].Value
$archive = Join-Path $packagesDirectory "$packageFileName.zip"
if (-not (Test-Path -LiteralPath $archive -PathType Leaf)) {
    throw "CPack did not produce the expected archive: $archive"
}

$archiveInfo = Get-Item -LiteralPath $archive
$archiveHash = Get-FileHash -LiteralPath $archive -Algorithm SHA256
Write-Host "Portable simulator package: $($archiveInfo.FullName)"
Write-Host "Archive size: $($archiveInfo.Length) bytes"
Write-Host "SHA256: $($archiveHash.Hash)"

<#
.SYNOPSIS
    Build and/or package the air_curve_conv project using CMake.

.DESCRIPTION
    This script automates the build process for the air_curve_conv project.

.PARAMETER Preset
    The CMake preset to use for building the project. Default is 'release'.

.PARAMETER Build
    If specified, the script will build the project using the specified CMake preset.

.PARAMETER Release
    If specified, the script will prepare the project for release by copying necessary files and creating a zip package.
#>

param (
    [Alias('p')]
    [string] $Preset = 'release',

    [Alias('b')]
    [switch] $Build,

    [Alias('r')]
    [switch] $Release
)

function Invoke-Checked {
    param([string] $CommandLine)

    Write-Host "-> $CommandLine"
    cmd /c $CommandLine
    if ($LASTEXITCODE -ne 0) {
        throw "Command FAILED (exit $LASTEXITCODE): $CommandLine"
    }
}

function Import-VcVars64 {
    Write-Host "`n==> Importing vcvars64 environment ..."

    $vswhere = Join-Path "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer" "vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Write-Host "    vswhere.exe not found - install Visual Studio or add vswhere to PATH."
        return
    }

    $vsPath = & $vswhere -latest -products * -property installationPath -nologo
    if (-not $vsPath) { 
        Write-Host "    Visual Studio installation not found."
        return
    }

    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) { 
        Write-Host "    vcvars64.bat not found in $vsPath."
        return
    }
    
    try {
        $output = cmd /c "`"$vcvars`" 2>&1 && set" | Out-String
        $output -split "`r`n" | ForEach-Object {
            if ($_ -match '^([^=]+)=(.*)') {
                [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
            }
        }
        Write-Host "VCVARS64 environment loaded successfully."
    }
    catch {
        Write-Host "Failed to load VCVARS64: $_" -ForegroundColor Red
    }
}

if (-not $Build -and -not $Publish) {
    $Build = $true
    $Publish = $true
}

$ErrorActionPreference = 'Stop'

try {
    if ($Build) { Import-VcVars64 }

    $ProjectName = (Get-Content -Path (Join-Path $PSScriptRoot 'config/PROJECT') -Raw).Trim()
    $ProjectVersion = (Get-Content -Path (Join-Path $PSScriptRoot 'config/VERSION') -Raw).Trim()

    $RepoRoot = $PSScriptRoot
    $OutDir = Join-Path $RepoRoot 'out'
    $BuildDir = Join-Path $OutDir 'build'
    $BinDir = Join-Path $BuildDir $Preset
    $PublishDir = Join-Path $OutDir 'publish'
    $PublishZip = Join-Path $OutDir "$ProjectName-$ProjectVersion-$Preset.zip"

    if ($Build) {
        Write-Host "`n==> Configuring & Building '$Preset'"
        Invoke-Checked "cmake --preset $Preset"
        Invoke-Checked "cmake --build --preset $Preset"
    }

    if ($Publish) {
        Write-Host "`n==> Preparing '$PublishDir'"
        Remove-Item $PublishDir -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Path $PublishDir | Out-Null

        Write-Host "==> Copying"
        Copy-Item (Join-Path $BinDir 'main.dll') (Join-Path $PublishDir "$ProjectName.dll")
        Copy-Item (Join-Path $BinDir 'main.ini') (Join-Path $PublishDir "$ProjectName.ini")

        $TrackedFiles = git -C $RepoRoot ls-files aff
        foreach ($File in $TrackedFiles) {
            if ($File -like '.gitignore') { continue }
            $SourcePath = Join-Path $RepoRoot   $File
            $DestinationPath = Join-Path $PublishDir $File
            $DestinationDir = Split-Path $DestinationPath -Parent
            if (-not (Test-Path $DestinationDir)) {
                New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null
            }
            Copy-Item $SourcePath $DestinationPath
        }

        Write-Host "==> Packaging '$PublishZip'"
        if (Test-Path $PublishZip) { Remove-Item $PublishZip -Force }
        Compress-Archive -Path "$PublishDir\*" -DestinationPath $PublishZip -CompressionLevel Optimal
    }

    Write-Host "`n==> Done."
}
catch {
    throw
}
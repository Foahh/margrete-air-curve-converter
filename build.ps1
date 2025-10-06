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

$ErrorActionPreference = 'Stop'

try {
    Import-VcVars64

    $ProjectName = (Get-Content -Path (Join-Path $PSScriptRoot 'config/PROJECT') -Raw).Trim()
    $ProjectVersion = (Get-Content -Path (Join-Path $PSScriptRoot 'config/VERSION') -Raw).Trim()

    $Target = 'Release'
    $RepoRoot = $PSScriptRoot
    $BuildDir = Join-Path $RepoRoot 'build'
    $PublishDir = Join-Path $RepoRoot 'publish'
    $PublishZip = Join-Path $PublishDir "$ProjectName-$ProjectVersion-$Target.zip"

    Write-Host "`n==> Configuring & Building $Target"
    Invoke-Checked "cmake --preset vcpkg"
    Invoke-Checked "cmake --build build --config $Target"

    Write-Host "`n==> Preparing '$PublishDir'"
    Remove-Item $PublishDir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Path $PublishDir | Out-Null

    Write-Host "==> Copying"
    $sourceDll = Join-Path (Join-Path $BuildDir -ChildPath $Target) -ChildPath 'main.dll'
    $targetDll = Join-Path $PublishDir -ChildPath "$ProjectName.dll"
    Copy-Item $sourceDll $targetDll

    $sourceIni = Join-Path $BuildDir -ChildPath 'main.ini'
    $targetIni = Join-Path $PublishDir -ChildPath "$ProjectName.ini"
    Copy-Item $sourceIni $targetIni

    $TrackedFiles = git -C $RepoRoot ls-files aff
    foreach ($File in $TrackedFiles) {
        if ($File -like '.gitignore') { continue }
        $SourcePath = Join-Path $RepoRoot -ChildPath $File
        $DestinationPath = Join-Path $PublishDir -ChildPath $File
        $DestinationDir = Split-Path $DestinationPath -Parent
        if (-not (Test-Path $DestinationDir)) {
            New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null
        }
        Copy-Item $SourcePath $DestinationPath
    }

    Write-Host "==> Packaging '$PublishZip'"
    if (Test-Path $PublishZip) { Remove-Item $PublishZip -Force }
    Compress-Archive -Path "$PublishDir\*" -DestinationPath $PublishZip -CompressionLevel Optimal
 
    Write-Host "`n==> Done."
}
catch {
    throw
}
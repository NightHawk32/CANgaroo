param(
    [string]$QtBin = "",
    [switch]$WithPython,
    [switch]$EnablePeakCan,
    [switch]$EnableKvaser,
    [string]$CanlibDir = "",
    [string]$OutputRoot = "",
    [switch]$Clean,
    [switch]$Zip
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-Tool {
    param(
        [string[]]$Names
    )

    foreach ($name in $Names) {
        $cmd = Get-Command -Name $name -ErrorAction SilentlyContinue
        if ($cmd) {
            return $cmd.Source
        }
    }
    return $null
}

function Resolve-Pybind11Include {
    $candidates = @(
        @{ exe = "py";      args = @("-3", "-c", "import pybind11; print(pybind11.get_include())") },
        @{ exe = "python";  args = @("-c", "import pybind11; print(pybind11.get_include())") },
        @{ exe = "python3"; args = @("-c", "import pybind11; print(pybind11.get_include())") }
    )

    foreach ($candidate in $candidates) {
        $exePath = Resolve-Tool -Names @($candidate.exe)
        if (-not $exePath) {
            continue
        }

        try {
            $output = & $exePath @($candidate.args) 2>$null
            if ($LASTEXITCODE -ne 0 -or -not $output) {
                continue
            }

            $includeDir = ($output | Select-Object -First 1).Trim()
            if ($includeDir -and (Test-Path $includeDir)) {
                return (Resolve-Path $includeDir).Path
            }
        }
        catch {
            continue
        }
    }

    return $null
}

function Resolve-PythonEmbedPaths {
    # Prefer Strawberry Python for MinGW builds because it provides GCC-compatible import libraries.
    $strawberryRoot = "C:/Strawberry/c"
    $candidateIncludes = @(
        (Join-Path $strawberryRoot "include/python3.9"),
        (Join-Path $strawberryRoot "include/python3.10"),
        (Join-Path $strawberryRoot "include/python3.11"),
        (Join-Path $strawberryRoot "include/python3.12"),
        (Join-Path $strawberryRoot "include/python3.13")
    )

    $candidateLibDirs = @(
        (Join-Path $strawberryRoot "lib")
    )

    foreach ($inc in $candidateIncludes) {
        if (-not (Test-Path (Join-Path $inc "Python.h"))) {
            continue
        }

        foreach ($libDir in $candidateLibDirs) {
            if (-not (Test-Path $libDir)) {
                continue
            }

            $libFile = Get-ChildItem -Path $libDir -Filter "libpython3*.a" -ErrorAction SilentlyContinue |
                       Select-Object -First 1
            if (-not $libFile) {
                continue
            }

            $libBase = [IO.Path]::GetFileNameWithoutExtension($libFile.Name)
            $libName = $libBase
            if ($libName.StartsWith("lib")) {
                $libName = $libName.Substring(3)
            }

            return @{
                Include = (Resolve-Path $inc).Path
                LibDir  = (Resolve-Path $libDir).Path
                LibName = $libName
            }
        }
    }

    return $null
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repoRoot "dist\win64"
}

$buildRoot = Join-Path $repoRoot "build\win64-release"
$bundleDir = Join-Path $OutputRoot "cangaroo-win64"
$compatIncludeDir = Join-Path $buildRoot "compat\include"

if ($Clean) {
    if (Test-Path $buildRoot) {
        Remove-Item -Path $buildRoot -Recurse -Force
    }
    if (Test-Path $bundleDir) {
        Remove-Item -Path $bundleDir -Recurse -Force
    }
}

New-Item -Path $buildRoot -ItemType Directory -Force | Out-Null
New-Item -Path $bundleDir -ItemType Directory -Force | Out-Null
New-Item -Path $compatIncludeDir -ItemType Directory -Force | Out-Null

$compatCryptHeader = Join-Path $compatIncludeDir "crypt.h"
if (-not (Test-Path $compatCryptHeader)) {
    @'
#ifndef CANGAROO_COMPAT_CRYPT_H
#define CANGAROO_COMPAT_CRYPT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compatibility shim for toolchains where Python headers expect <crypt.h>
 * but the MinGW/Windows SDK does not provide it.
 */
static __inline char *crypt(const char *key, const char *salt)
{
    (void)key;
    (void)salt;
    return (char *)0;
}

#ifdef __cplusplus
}
#endif

#endif /* CANGAROO_COMPAT_CRYPT_H */
'@ | Set-Content -Path $compatCryptHeader -Encoding ascii
}

$qmakePath = $null
if (-not [string]::IsNullOrWhiteSpace($QtBin)) {
    $candidate = Join-Path $QtBin "qmake.exe"
    if (-not (Test-Path $candidate)) {
        throw "qmake.exe was not found in QtBin: $QtBin"
    }
    $qmakePath = $candidate
} else {
    $qmakePath = Resolve-Tool -Names @("qmake.exe", "qmake6.exe", "qmake")
}

if (-not $qmakePath) {
    throw "qmake was not found. Install Qt6 and add its bin directory to PATH, or pass -QtBin <path>."
}

$qtPrefixDir = Split-Path -Parent (Split-Path -Parent $qmakePath)
$qtVersionDir = Split-Path -Parent $qtPrefixDir
$qtRootDir = Split-Path -Parent $qtVersionDir

$qtMingwBin = $null
$qtMingwGxxCandidates = @(Get-ChildItem -Path (Join-Path $qtRootDir "Tools") -Filter "g++.exe" -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match "Tools[\\/]mingw.*_64[\\/]bin[\\/]g\+\+\.exe$" } |
    Sort-Object -Property FullName -Descending)

if ($qtMingwGxxCandidates -and $qtMingwGxxCandidates.Count -gt 0) {
    $qtMingwBin = Split-Path -Parent $qtMingwGxxCandidates[0].FullName
    if ($env:PATH -notlike "$qtMingwBin;*") {
        $env:PATH = "$qtMingwBin;$env:PATH"
    }
    Write-Host "Using Qt MinGW toolchain: $qtMingwBin"
}

$qtArch = (& $qmakePath -query QT_ARCH 2>$null).Trim()
$qtInstallPrefix = (& $qmakePath -query QT_INSTALL_PREFIX).Trim()
$qmakeSpec = (& $qmakePath -query QMAKE_SPEC).Trim()

$archFingerprint = ("$qtArch;$qtInstallPrefix;$qmakeSpec").ToLowerInvariant()
$is64Qt = $archFingerprint -match "x86_64|amd64|_64|/64|\\64"

if (-not $is64Qt) {
    throw "The selected Qt toolchain does not look like 64-bit (QT_ARCH=$qtArch, QT_INSTALL_PREFIX=$qtInstallPrefix, QMAKE_SPEC=$qmakeSpec). Use a 64-bit Qt kit/toolchain."
}

$qtHostBins = (& $qmakePath -query QT_HOST_BINS).Trim()
$windeployqtPath = $null
if (-not [string]::IsNullOrWhiteSpace($QtBin)) {
    $windeployqtPath = Join-Path $QtBin "windeployqt.exe"
} elseif (-not [string]::IsNullOrWhiteSpace($qtHostBins)) {
    $windeployqtPath = Join-Path $qtHostBins "windeployqt.exe"
}

if (-not (Test-Path $windeployqtPath)) {
    $windeployqtPath = Resolve-Tool -Names @("windeployqt.exe", "windeployqt")
}

if (-not $windeployqtPath) {
    throw "windeployqt was not found. Install Qt tools and ensure windeployqt.exe is available."
}

$makeCandidates = @("jom.exe", "jom", "nmake.exe", "nmake", "mingw32-make.exe", "mingw32-make", "make.exe", "make")
if ($qmakeSpec -match "g\+\+|clang") {
    $makeCandidates = @("mingw32-make.exe", "mingw32-make", "make.exe", "make", "jom.exe", "jom", "nmake.exe", "nmake")
} elseif ($qmakeSpec -match "msvc") {
    $makeCandidates = @("jom.exe", "jom", "nmake.exe", "nmake", "mingw32-make.exe", "mingw32-make", "make.exe", "make")
}

$makeTool = $null
if ($qtMingwBin) {
    $qtMake = Join-Path $qtMingwBin "mingw32-make.exe"
    if (Test-Path $qtMake) {
        $makeTool = $qtMake
    }
}

if (-not $makeTool) {
    $makeTool = Resolve-Tool -Names $makeCandidates
}
if (-not $makeTool) {
    throw "No supported make tool found (jom, nmake, mingw32-make, make)."
}

$qmakeArgs = @(
    (Join-Path $repoRoot "cangaroo.pro"),
    "CONFIG+=release"
)

if (-not $WithPython) {
    $qmakeArgs += "CONFIG+=skip_python"
    Write-Host "Python embedding is disabled for this build (CONFIG+=skip_python). Use -WithPython to enable it."
}

$normCompatInclude = ((Resolve-Path $compatIncludeDir).Path) -replace "\\", "/"
$qmakeArgs += "INCLUDEPATH+=$normCompatInclude"

if ($WithPython) {
    $pybind11Include = Resolve-Pybind11Include
    if ($pybind11Include) {
        # Command-line INCLUDEPATH works around qmake/python3 lookup issues on some Windows setups.
        $normalizedPybindInclude = $pybind11Include -replace "\\", "/"
        $qmakeArgs += "INCLUDEPATH+=$normalizedPybindInclude"
        Write-Host "Using pybind11 include: $pybind11Include"
    } else {
        Write-Warning "pybind11 include directory was not auto-detected. Build may fail unless pybind11 is available in qmake INCLUDEPATH."
    }
}

if ($EnablePeakCan) {
    $qmakeArgs += "CONFIG+=peakcan"
}

if ($EnableKvaser) {
    $qmakeArgs += "CONFIG+=kvaser"
    if (-not [string]::IsNullOrWhiteSpace($CanlibDir)) {
        $qmakeArgs += "CANLIB_DIR=$CanlibDir"
    }
}

if ($WithPython) {
    $pythonEmbed = Resolve-PythonEmbedPaths
    if ($pythonEmbed) {
        $normPyInc = $pythonEmbed.Include -replace "\\", "/"
        $normPyLib = $pythonEmbed.LibDir -replace "\\", "/"
        $qmakeArgs += "INCLUDEPATH+=$normPyInc"
        $qmakeArgs += "LIBS+=-L$normPyLib"
        $qmakeArgs += "LIBS+=-l$($pythonEmbed.LibName)"
        Write-Host "Using Python embed include: $($pythonEmbed.Include)"
        Write-Host "Using Python embed lib dir:  $($pythonEmbed.LibDir)"
        Write-Host "Using Python embed library: $($pythonEmbed.LibName)"
    } else {
        Write-Warning "Could not auto-detect Python embed headers/libs (Python.h + libpython3*.a). Build may fail in PythonEngine.cpp."
    }
}

Push-Location $buildRoot
try {
    Write-Host "[1/4] Running qmake..."
    & $qmakePath @qmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "qmake failed with exit code $LASTEXITCODE"
    }

    Write-Host "[2/4] Building release..."
    $cpuCount = [Math]::Max([Environment]::ProcessorCount, 1)
    $makeLeaf = [IO.Path]::GetFileName($makeTool).ToLowerInvariant()

    if ($makeLeaf -eq "jom.exe" -or $makeLeaf -eq "jom") {
        & $makeTool "/J" "$cpuCount"
    } elseif ($makeLeaf -eq "mingw32-make.exe" -or $makeLeaf -eq "mingw32-make" -or $makeLeaf -eq "make.exe" -or $makeLeaf -eq "make") {
        & $makeTool "-j$cpuCount"
    } else {
        & $makeTool
    }

    if ($LASTEXITCODE -ne 0) {
        throw "build failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

$exeCandidates = @(
    (Join-Path $repoRoot "bin\cangaroo.exe"),
    (Join-Path $buildRoot "bin\cangaroo.exe")
)

$exePath = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exePath) {
    throw "Expected executable not found. Checked: $($exeCandidates -join ', ')"
}

Write-Host "[3/4] Copying executable and metadata..."
Copy-Item -Path $exePath -Destination (Join-Path $bundleDir "cangaroo.exe") -Force

$metadataFiles = @("README.md", "LICENSE")
foreach ($fileName in $metadataFiles) {
    $full = Join-Path $repoRoot $fileName
    if (Test-Path $full) {
        Copy-Item -Path $full -Destination (Join-Path $bundleDir $fileName) -Force
    }
}

Write-Host "[4/4] Running windeployqt..."
& $windeployqtPath "--release" "--compiler-runtime" "--dir" $bundleDir (Join-Path $bundleDir "cangaroo.exe")
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

if ($Zip) {
    $zipPath = Join-Path $OutputRoot "cangaroo-win64.zip"
    if (Test-Path $zipPath) {
        Remove-Item -Path $zipPath -Force
    }
    Compress-Archive -Path (Join-Path $bundleDir "*") -DestinationPath $zipPath -Force
    Write-Host "Bundle archive created: $zipPath"
}

Write-Host "Build and bundle completed successfully."
Write-Host "Bundle output: $bundleDir"
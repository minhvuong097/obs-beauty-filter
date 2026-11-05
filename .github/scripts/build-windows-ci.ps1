$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$ObsVersion = "33.0.1"
$DepsVersion = "2026-10-23"
$DepsSha256 = "7508972a867a0ef68c26cd8bdba61fd999edd0a92fa9eb9585774de9bf51bfbe"

$RootDir = (Resolve-Path "$PSScriptRoot\..\..").Path
$CiDir = Join-Path $RootDir ".ci"
$DepsDir = Join-Path $CiDir "deps"
$ObsSrcDir = Join-Path $CiDir "obs-studio-$ObsVersion"
$ObsBuildDir = Join-Path $CiDir "obs-build"
$ObsSdkDir = Join-Path $CiDir "obs-sdk"
$BuildDir = Join-Path $RootDir "build"
$PackageDir = Join-Path $RootDir "package"

New-Item -ItemType Directory -Force -Path $CiDir | Out-Null

function Download-File {
  param(
    [Parameter(Mandatory = $true)][string]$Uri,
    [Parameter(Mandatory = $true)][string]$OutFile
  )

  if (Test-Path $OutFile) {
    return
  }

  Write-Host "Downloading $Uri"
  Invoke-WebRequest -Uri $Uri -OutFile $OutFile
}

if (-not (Test-Path (Join-Path $DepsDir "include"))) {
  $DepsZip = Join-Path $CiDir "windows-deps-$DepsVersion-x64.zip"
  Download-File `
    -Uri "https://github.com/obsproject/obs-deps/releases/download/$DepsVersion/windows-deps-$DepsVersion-x64.zip" `
    -OutFile $DepsZip

  $ActualHash = (Get-FileHash -Algorithm SHA256 $DepsZip).Hash.ToLowerInvariant()
  if ($ActualHash -ne $DepsSha256) {
    throw "OBS dependencies checksum mismatch: $ActualHash"
  }

  New-Item -ItemType Directory -Force -Path $DepsDir | Out-Null
  Expand-Archive -Path $DepsZip -DestinationPath $DepsDir -Force
}

if (-not (Test-Path (Join-Path $ObsSrcDir "CMakeLists.txt"))) {
  $ObsZip = Join-Path $CiDir "obs-studio-$ObsVersion.zip"
  Download-File `
    -Uri "https://github.com/obsproject/obs-studio/archive/refs/tags/$ObsVersion.zip" `
    -OutFile $ObsZip
  Expand-Archive -Path $ObsZip -DestinationPath $CiDir -Force
}

# OBS 33 installs libobsConfig.cmake into <prefix>/cmake on Windows (the
# target_export() helper uses OBS_CMAKE_DESTINATION, which is "cmake").  Older
# layouts used <prefix>/lib/cmake/libobs, so accept both when looking for an
# existing SDK.
$ObsSdkConfigCandidates = @(
  (Join-Path $ObsSdkDir "cmake\libobsConfig.cmake"),
  (Join-Path $ObsSdkDir "lib\cmake\libobs\libobsConfig.cmake")
)

if (-not ($ObsSdkConfigCandidates | Where-Object { Test-Path $_ })) {
  cmake -S $ObsSrcDir -B $ObsBuildDir `
    -G "Visual Studio 17 2022" -A x64 `
    "-DCMAKE_PREFIX_PATH=$DepsDir" `
    -DOBS_CMAKE_VERSION=3.0.0 `
    "-DOBS_VERSION_OVERRIDE=$ObsVersion" `
    -DENABLE_CORE_MODULES=OFF `
    -DENABLE_FRONTEND=OFF `
    -DENABLE_SCRIPTING=OFF `
    -DCMAKE_SYSTEM_VERSION=10.0.22621.0
  if ($LASTEXITCODE -ne 0) { throw "OBS configure failed" }

  cmake --build $ObsBuildDir --config RelWithDebInfo --target libobs libobs-d3d11 obs-frontend-api --parallel
  if ($LASTEXITCODE -ne 0) { throw "OBS libobs build failed" }

  # Installing the Development component produces the SDK tree consumed by
  # find_package(libobs): libobsConfig.cmake + libobsTargets.cmake in
  # <prefix>/cmake, headers in <prefix>/include and import libraries in
  # <prefix>/lib.
  cmake --install $ObsBuildDir `
    --config RelWithDebInfo `
    --component Development `
    --prefix $ObsSdkDir
  if ($LASTEXITCODE -ne 0) { throw "OBS libobs install failed" }
}

if (-not ($ObsSdkConfigCandidates | Where-Object { Test-Path $_ })) {
  throw "libobsConfig.cmake was not installed into $ObsSdkDir"
}
if (-not (Test-Path (Join-Path $ObsSdkDir "cmake\w32-pthreadsConfig.cmake"))) {
  throw "w32-pthreadsConfig.cmake is missing from the OBS SDK in $ObsSdkDir"
}

if (Test-Path $BuildDir) {
  Remove-Item -Recurse -Force $BuildDir
}

cmake -S $RootDir -B $BuildDir `
  -G "Visual Studio 17 2022" -A x64 `
  "-DCMAKE_PREFIX_PATH=$DepsDir;$ObsSdkDir" `
  -DBEAUTY_BUILD_TESTS=ON `
  -DBEAUTY_OBS33_PLUGIN_LAYOUT=ON
if ($LASTEXITCODE -ne 0) { throw "Plugin configure failed" }

cmake --build $BuildDir --config RelWithDebInfo --parallel
if ($LASTEXITCODE -ne 0) { throw "Plugin build failed" }

& (Join-Path $BuildDir "RelWithDebInfo\beauty-face-region-test.exe")
if ($LASTEXITCODE -ne 0) { throw "Face-region test failed" }

# Exercise libobs-d3d11 and compile the effect on the real Windows graphics
# path.  The full render matrix runs on Linux/OpenGL; this basic invocation
# catches D3D11 shader compilation and module-loading regressions.  OBS keeps
# its runtime DLLs in the build's rundir and its effect files in the source
# tree, so put both on the process search/data paths.  os_dlopen() uses
# LOAD_LIBRARY_SEARCH_DEFAULT_DIRS, which ignores PATH, so the graphics module
# must sit next to the test executable.
$ObsBinDir = Join-Path $ObsBuildDir "rundir\RelWithDebInfo\bin\64bit"
$env:PATH = "$ObsBinDir;$(Join-Path $DepsDir 'bin');$env:PATH"
$env:OBS_BEAUTY_DATA_PATH = (Join-Path $ObsSrcDir "libobs\data") + "\"

$LibobsD3d11 = Get-ChildItem -Path $ObsBuildDir -Recurse -Filter "libobs-d3d11.dll" |
  Select-Object -First 1
if (-not $LibobsD3d11) { throw "libobs-d3d11.dll was not built" }
Copy-Item $LibobsD3d11.FullName (Join-Path $BuildDir "RelWithDebInfo")

& (Join-Path $BuildDir "RelWithDebInfo\beauty-load-test.exe") `
  (Join-Path $BuildDir "RelWithDebInfo\obs-beauty-filter.dll") `
  (Join-Path $BuildDir "rundir\plugins\obs-beauty-filter\data")
if ($LASTEXITCODE -ne 0) { throw "Windows module load / effect compilation test failed" }

Remove-Item Env:\OBS_BEAUTY_DATA_PATH

if (Test-Path $PackageDir) {
  Remove-Item -Recurse -Force $PackageDir
}

cmake --install $BuildDir --config RelWithDebInfo --prefix $PackageDir
if ($LASTEXITCODE -ne 0) { throw "Plugin install failed" }

$InstalledDll = Join-Path $PackageDir "obs-beauty-filter\obs-beauty-filter.dll"
if (-not (Test-Path $InstalledDll)) {
  throw "Installed plugin DLL not found at $InstalledDll"
}

$VersionMatch = Select-String -Path (Join-Path $RootDir "CMakeLists.txt") `
  -Pattern 'project\(obs-beauty-filter VERSION ([0-9]+\.[0-9]+\.[0-9]+)'
if (-not $VersionMatch) { throw "Cannot determine plugin version" }
$PluginVersion = $VersionMatch.Matches[0].Groups[1].Value

$ZipPath = Join-Path $RootDir "obs-beauty-filter-$PluginVersion-windows-x64.zip"
if (Test-Path $ZipPath) {
  Remove-Item -Force $ZipPath
}
Compress-Archive -Path (Join-Path $PackageDir "obs-beauty-filter") -DestinationPath $ZipPath
Write-Host "Created $ZipPath"

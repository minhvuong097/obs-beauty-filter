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
$DepsZip = Join-Path $CiDir "windows-deps-$DepsVersion-x64.zip"

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

# Downloads one asset of a GitHub release through the REST API instead of the
# browser URL.  GitHub's CDN has returned HTTP 504 for individual release
# assets while the API asset endpoint kept working, so this is our fallback.
function Save-GitHubReleaseAsset {
  param(
    [Parameter(Mandatory = $true)][string]$Repo,
    [Parameter(Mandatory = $true)][string]$Tag,
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$OutFile
  )

  if (Test-Path $OutFile) {
    return
  }

  $Headers = @{ Accept = "application/vnd.github+json" }
  if ($env:GITHUB_TOKEN) {
    $Headers.Authorization = "Bearer $($env:GITHUB_TOKEN)"
  }

  $Release = Invoke-RestMethod -Headers $Headers `
    -Uri "https://api.github.com/repos/$Repo/releases/tags/$Tag"
  $Asset = $Release.assets | Where-Object { $_.name -eq $Name } |
    Select-Object -First 1
  if (-not $Asset) {
    throw "Release asset $Name was not found in $Repo@$Tag"
  }

  $TempFile = "$OutFile.part"
  Remove-Item -Force $TempFile -ErrorAction SilentlyContinue
  Write-Host "Downloading $Name through the GitHub API ($($Asset.size) bytes)"
  & curl.exe -fsSL --retry 3 --retry-delay 2 `
    -H "Accept: application/octet-stream" `
    -H "User-Agent: obs-beauty-filter-ci" `
    -o $TempFile $Asset.url
  if ($LASTEXITCODE -ne 0) {
    Remove-Item -Force $TempFile -ErrorAction SilentlyContinue
    throw "Failed to download $Name through the GitHub API"
  }

  if ($Asset.digest -and $Asset.digest.StartsWith("sha256:")) {
    $Expected = $Asset.digest.Substring("sha256:".Length)
    $Actual = (Get-FileHash -Algorithm SHA256 $TempFile).Hash.ToLowerInvariant()
    if ($Actual -ne $Expected) {
      Remove-Item -Force $TempFile
      throw "Checksum mismatch for ${Name}: expected $Expected, got $Actual"
    }
  }

  Move-Item -Force $TempFile $OutFile
}

if (-not (Test-Path (Join-Path $DepsDir "include"))) {
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

# OBS 33's Windows x64 configure unconditionally configures a Win32 child tree
# (build_x86) for the virtual-camera and capture helpers, and that child pulls
# the small x86 prebuilt deps archive even when only x64 targets are built.
# GitHub's CDN has answered the browser download URL for that archive with HTTP
# 504, so fetch it up front and seed OBS' .deps cache: buildspec.cmake reuses an
# existing archive instead of downloading it again.
$ObsDepsCache = Join-Path $ObsSrcDir ".deps"
New-Item -ItemType Directory -Force -Path $ObsDepsCache | Out-Null

$ObsX86DepsZip = Join-Path $ObsDepsCache "windows-deps-$DepsVersion-x86.zip"
if (-not (Test-Path $ObsX86DepsZip)) {
  $BrowserUri = "https://github.com/obsproject/obs-deps/releases/download/$DepsVersion/windows-deps-$DepsVersion-x86.zip"
  $TempZip = "$ObsX86DepsZip.part"
  $Downloaded = $false

  for ($Attempt = 1; $Attempt -le 3 -and -not $Downloaded; $Attempt++) {
    try {
      Write-Host "Downloading $BrowserUri (attempt $Attempt)"
      Invoke-WebRequest -Uri $BrowserUri -OutFile $TempZip -UseBasicParsing
      $Downloaded = $true
    } catch {
      Remove-Item -Force $TempZip -ErrorAction SilentlyContinue
      Write-Host "  attempt $Attempt failed: $($_.Exception.Message)"
    }
  }

  if ($Downloaded) {
    Move-Item -Force $TempZip $ObsX86DepsZip
  } else {
    Save-GitHubReleaseAsset -Repo "obsproject/obs-deps" -Tag $DepsVersion `
      -Name "windows-deps-$DepsVersion-x86.zip" -OutFile $ObsX86DepsZip
  }
  Write-Host "Seeded $(Split-Path -Leaf $ObsX86DepsZip) into $ObsDepsCache"
}

# Reuse the x64 archive we already downloaded instead of letting OBS fetch the
# same 64 MB file again from the release CDN.
$ObsX64DepsZip = Join-Path $ObsDepsCache "windows-deps-$DepsVersion-x64.zip"
if (-not (Test-Path $ObsX64DepsZip) -and (Test-Path $DepsZip)) {
  Copy-Item $DepsZip $ObsX64DepsZip
  Write-Host "Seeded $(Split-Path -Leaf $ObsX64DepsZip) into $ObsDepsCache"
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
# path.  The render matrix also runs here (without the Linux golden image) to
# catch D3D11 shader compilation, resolution and alpha regressions.  OBS keeps
# its runtime DLLs in the build's rundir and its effect files in the source
# tree, so put both on the process search/data paths.  os_dlopen() uses
# LOAD_LIBRARY_SEARCH_DEFAULT_DIRS, which ignores PATH, so the graphics module
# must sit next to the test executable.
$ObsBinDir = Join-Path $ObsBuildDir "rundir\RelWithDebInfo\bin\64bit"
$env:PATH = "$ObsBinDir;$(Join-Path $DepsDir 'bin');$env:PATH"
# OBS' effect preprocessor inserts include paths by looking for the last '/'.
# Use forward slashes so #include "color.effect" also resolves on Windows.
$ObsDataDir = (Join-Path $ObsSrcDir "libobs\data") -replace '\\', '/'
$env:OBS_BEAUTY_DATA_PATH = $ObsDataDir + "/"

& (Join-Path $BuildDir "RelWithDebInfo\beauty-preset-test.exe")
if ($LASTEXITCODE -ne 0) { throw "Preset test failed" }

$LibobsD3d11 = Get-ChildItem -Path $ObsBuildDir -Recurse -Filter "libobs-d3d11.dll" |
  Select-Object -First 1
if (-not $LibobsD3d11) { throw "libobs-d3d11.dll was not built" }
Copy-Item $LibobsD3d11.FullName (Join-Path $BuildDir "RelWithDebInfo")

& (Join-Path $BuildDir "RelWithDebInfo\beauty-load-test.exe") `
  (Join-Path $BuildDir "RelWithDebInfo\obs-beauty-filter.dll") `
  (Join-Path $BuildDir "rundir\plugins\obs-beauty-filter\data")
if ($LASTEXITCODE -ne 0) { throw "Windows module load / effect compilation test failed" }

# Resolution/alpha/16F invariants on D3D11.  The golden image is intentionally
# not compared here: it is generated on Linux/llvmpipe and would not match
# D3D11 rounding.
& (Join-Path $BuildDir "RelWithDebInfo\beauty-render-matrix-test.exe") `
  (Join-Path $BuildDir "RelWithDebInfo\obs-beauty-filter.dll") `
  (Join-Path $BuildDir "rundir\plugins\obs-beauty-filter\data")
if ($LASTEXITCODE -ne 0) { throw "Windows render matrix test failed" }

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
  -Pattern 'set\(BEAUTY_FILTER_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"\)'
if (-not $VersionMatch) { throw "Cannot determine plugin version" }
$PluginVersion = $VersionMatch.Matches[0].Groups[1].Value

$ZipPath = Join-Path $RootDir "obs-beauty-filter-$PluginVersion-windows-x64.zip"
if (Test-Path $ZipPath) {
  Remove-Item -Force $ZipPath
}
Compress-Archive -Path (Join-Path $PackageDir "obs-beauty-filter") -DestinationPath $ZipPath
Write-Host "Created $ZipPath"

# ---------------------------------------------------------------------------
# Windows installer (.exe)
# ---------------------------------------------------------------------------
# The installer bundles the same package/obs-beauty-filter tree as the ZIP and
# writes it to %ProgramData%\obs-studio\plugins\obs-beauty-filter (or
# <portable-OBS>\plugins\obs-beauty-filter when a portable tree is detected).
# Inno Setup 6 is preinstalled on some runners and available through Chocolatey
# on the GitHub-hosted Windows images.
$IsccCandidates = @(
  (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"),
  (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe")
)
$Iscc = $IsccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Iscc) {
  Write-Host "Installing Inno Setup via Chocolatey"
  choco install innosetup -y --no-progress
  $Iscc = $IsccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Iscc) {
  throw "ISCC.exe was not found after installing Inno Setup"
}

$SetupPath = Join-Path $RootDir "obs-beauty-filter-$PluginVersion-windows-x64-setup.exe"
if (Test-Path $SetupPath) {
  Remove-Item -Force $SetupPath
}

$IssPath = Join-Path $RootDir "installer\obs-beauty-filter.iss"
& $Iscc `
  "/DPluginVersion=$PluginVersion" `
  "/DPluginPackageDir=$PackageDir\obs-beauty-filter" `
  "/DOutputDir=$RootDir" `
  $IssPath
if ($LASTEXITCODE -ne 0) { throw "Installer build failed" }
if (-not (Test-Path $SetupPath)) {
  throw "Installer was not created at $SetupPath"
}
Write-Host "Created $SetupPath"

# Installer smoke test.  There is no OBS installation on the CI runner, so the
# installer takes the default OBS 33+ layout; /DIR redirects the plugin folder
# to a temporary directory.  Verify the payload, uninstall silently, and make
# sure nothing is left behind.
$SmokeDir = Join-Path $RootDir "installer-smoke"
$SmokeLog = Join-Path $RootDir "installer-smoke.log"
Remove-Item -Recurse -Force $SmokeDir -ErrorAction SilentlyContinue
Remove-Item -Force $SmokeLog -ErrorAction SilentlyContinue

$InstallProc = Start-Process -FilePath $SetupPath -Wait -PassThru -ArgumentList @(
  "/VERYSILENT",
  "/SUPPRESSMSGBOXES",
  "/NORESTART",
  "/SP-",
  "/DIR=$SmokeDir",
  "/LOG=$SmokeLog"
)
if ($InstallProc.ExitCode -ne 0) {
  throw "Silent installer run failed with exit code $($InstallProc.ExitCode). See $SmokeLog"
}

# Give the installer a moment to flush its files in case an elevated child
# process outlives the launcher on a runner with UAC enabled.
for ($i = 0; $i -lt 60 -and -not (Test-Path (Join-Path $SmokeDir "obs-beauty-filter.dll")); $i++) {
  Start-Sleep -Milliseconds 500
}

$ExpectedFiles = @(
  "obs-beauty-filter.dll",
  "data\beauty_filter.effect",
  "data\locale\en-US.ini",
  "data\locale\vi-VN.ini",
  "obs-beauty-filter.ico",
  "unins000.exe"
)
foreach ($RelativePath in $ExpectedFiles) {
  $FullPath = Join-Path $SmokeDir $RelativePath
  if (-not (Test-Path $FullPath)) {
    throw "Installer payload is missing $RelativePath"
  }
}

$UninstallProc = Start-Process -FilePath (Join-Path $SmokeDir "unins000.exe") -Wait -PassThru -ArgumentList @(
  "/VERYSILENT",
  "/SUPPRESSMSGBOXES",
  "/NORESTART"
)
if ($UninstallProc.ExitCode -ne 0) {
  throw "Silent uninstall failed with exit code $($UninstallProc.ExitCode)"
}

# Inno's uninstaller may finish cleanup just after the process exits.
for ($i = 0; $i -lt 30 -and (Test-Path (Join-Path $SmokeDir "obs-beauty-filter.dll")); $i++) {
  Start-Sleep -Milliseconds 500
}
if (Test-Path (Join-Path $SmokeDir "obs-beauty-filter.dll")) {
  throw "Uninstaller did not remove obs-beauty-filter.dll"
}
Remove-Item -Recurse -Force $SmokeDir -ErrorAction SilentlyContinue
Remove-Item -Force $SmokeLog -ErrorAction SilentlyContinue
Write-Host "Installer smoke test passed"

Get-FileHash -Algorithm SHA256 $ZipPath, $SetupPath |
  ForEach-Object { Write-Host "$($_.Hash.ToLowerInvariant())  $(Split-Path -Leaf $_.Path)" }

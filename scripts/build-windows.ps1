param(
  [string]$BuildDir = "$PSScriptRoot\..\build",
  [string]$Config = "RelWithDebInfo",
  [string]$Generator = "Visual Studio 17 2022",
  [string]$ObsSdk = "",
  [string]$ObsBuild = "",
  [ValidateSet("obs33", "legacy")]
  [string]$Layout = "obs33",
  [string]$InstallPrefix = "",
  [switch]$Package,
  [switch]$Installer,
  [string]$PackageDir = "$PSScriptRoot\..\dist"
)

$ErrorActionPreference = "Stop"
$RootDir = (Resolve-Path "$PSScriptRoot\..").Path

# Prefer the Visual Studio 2026 generator when both CMake and a local VS 2026
# installation support it (OBS Studio 33 is built with VS 2026).  An explicit
# -Generator always wins; otherwise fall back to Visual Studio 2022.
if (-not $PSBoundParameters.ContainsKey("Generator")) {
  $cmakeHelp = (& cmake --help) | Out-String
  if ($cmakeHelp -match "Visual Studio 18 2026") {
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vs2026 = $null
    if ($vsWhere -and (Test-Path $vsWhere)) {
      $vs2026 = & $vsWhere -latest -version "[18.0,19.0)" -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath 2>$null
    }
    if ($vs2026) {
      $Generator = "Visual Studio 18 2026"
    }
  }
}

$prefixPaths = @()
if ($ObsSdk) { $prefixPaths += $ObsSdk }
if ($ObsBuild) { $prefixPaths += $ObsBuild }

$layoutValue = if ($Layout -eq "obs33") { "ON" } else { "OFF" }

$cmakeArgs = @(
  "-S", $RootDir,
  "-B", $BuildDir,
  "-G", $Generator,
  "-A", "x64",
  "-DBEAUTY_OBS33_PLUGIN_LAYOUT=$layoutValue"
)

if ($prefixPaths.Count -gt 0) {
  $cmakeArgs += "-DCMAKE_PREFIX_PATH=$($prefixPaths -join ';')"
}

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

& cmake --build $BuildDir --config $Config
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

if ($InstallPrefix) {
  & cmake --install $BuildDir --config $Config --prefix $InstallPrefix
  if ($LASTEXITCODE -ne 0) { throw "Install failed" }
}

if ($Package -or $Installer) {
  $staging = Join-Path $BuildDir "package"
  if (Test-Path $staging) {
    Remove-Item -Recurse -Force $staging
  }

  & cmake --install $BuildDir --config $Config --prefix $staging
  if ($LASTEXITCODE -ne 0) { throw "Package staging failed" }

  $versionMatch = Select-String -Path (Join-Path $RootDir "CMakeLists.txt") `
    -Pattern 'project\(obs-beauty-filter VERSION ([0-9]+\.[0-9]+\.[0-9]+)'
  if (-not $versionMatch) { throw "Cannot determine plugin version from CMakeLists.txt" }
  $version = $versionMatch.Matches[0].Groups[1].Value

  New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null

  if ($Package) {
    $zipPath = Join-Path $PackageDir "obs-beauty-filter-$version-windows-x64.zip"
    if (Test-Path $zipPath) {
      Remove-Item -Force $zipPath
    }

    if ($Layout -eq "obs33") {
      $packageSource = Join-Path $staging "obs-beauty-filter"
    } else {
      $packageSource = Join-Path $staging "*"
    }

    Compress-Archive -Path $packageSource -DestinationPath $zipPath
    Write-Host "Package: $zipPath"
  }

  if ($Installer) {
    if ($Layout -ne "obs33") {
      throw "-Installer chỉ hỗ trợ layout obs33 (OBS Studio 33+)."
    }

    $isccCandidates = @(
      (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"),
      (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe")
    )
    $iscc = $isccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $iscc) {
      Write-Host "Chưa có Inno Setup, đang cài bằng Chocolatey..."
      choco install innosetup -y --no-progress
      $iscc = $isccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    }
    if (-not $iscc) { throw "Không tìm thấy ISCC.exe của Inno Setup 6." }

    $issPath = Join-Path $RootDir "installer\obs-beauty-filter.iss"
    & $iscc `
      "/DPluginVersion=$version" `
      "/DPluginPackageDir=$staging\obs-beauty-filter" `
      "/DOutputDir=$PackageDir" `
      $issPath
    if ($LASTEXITCODE -ne 0) { throw "Tạo bộ cài thất bại." }

    $setupPath = Join-Path $PackageDir "obs-beauty-filter-$version-windows-x64-setup.exe"
    if (-not (Test-Path $setupPath)) { throw "Không tìm thấy bộ cài tại $setupPath" }
    Write-Host "Installer: $setupPath"
  }
}

Write-Host ""
Write-Host "Build xong: $BuildDir"

if ($Layout -eq "obs33") {
  Write-Host "Layout: OBS Studio 33+ (không bị gắn nhãn Legacy)."
  Write-Host "Cài thủ công: copy thư mục obs-beauty-filter vào:"
  Write-Host "  $env:ProgramData\obs-studio\plugins\"
  Write-Host "Portable: copy vào <OBS-portable>\plugins\."
  Write-Host "Hoặc cài bằng CMake:"
  Write-Host "  cmake --install `"$BuildDir`" --config $Config --prefix `"$env:ProgramData\obs-studio\plugins`""
} else {
  Write-Host "Layout: Legacy (OBS 32 trở xuống). OBS 33 vẫn load nhưng sẽ gắn nhãn Legacy,"
  Write-Host "và OBS 34 sẽ ngừng load layout này. Nên dùng -Layout obs33 nếu bạn dùng OBS 33+."
  Write-Host "Cài thủ công: DLL vào obs-plugins\64bit, data vào data\obs-plugins\obs-beauty-filter."
}

if ($Package) {
  if ($Layout -eq "obs33") {
    Write-Host "Giải nén file zip vào thư mục plugins (hoặc <OBS-portable>\plugins)."
  } else {
    Write-Host "Giải nén file zip vào thư mục cài OBS, ví dụ C:\Program Files\obs-studio."
  }
}

if ($Installer) {
  Write-Host "Bộ cài .exe nằm trong $PackageDir; chạy để cài trực tiếp vào OBS 33+."
}

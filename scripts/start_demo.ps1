param(
  [switch]$WithDocker,
  [switch]$SkipFrontend,
  [switch]$NoBrowser
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path "$PSScriptRoot\.."
$cmake = "C:\Program Files\CMake\bin\cmake.exe"
$gcc = "D:/APP/Mingw-64/Mingw-64/ucrt64/bin/gcc.exe"
$gxx = "D:/APP/Mingw-64/Mingw-64/ucrt64/bin/g++.exe"
$buildDir = Join-Path $repoRoot "build\mingw-debug"
$demoClient = Join-Path $buildDir "demo_client.exe"
$frontendDir = Join-Path $repoRoot "web\admin-dashboard"
$composeDir = Join-Path $repoRoot "infra\compose"
$viteUrl = "http://localhost:5173"
$pidDir = Join-Path $repoRoot ".run"
$frontendPidFile = Join-Path $pidDir "frontend.pid"

function Assert-Tool($toolPath, $label) {
  if (-not (Test-Path $toolPath)) {
    throw "$label not found at: $toolPath"
  }
}

function Write-Step($message) {
  Write-Host ""
  Write-Host "== $message ==" -ForegroundColor Cyan
}

function Start-Frontend {
  if (-not (Test-Path (Join-Path $frontendDir "node_modules"))) {
    Write-Step "Installing frontend dependencies"
    Push-Location $frontendDir
    try {
      npm install
    } finally {
      Pop-Location
    }
  }

  New-Item -ItemType Directory -Force -Path $pidDir | Out-Null

  if (Test-Path $frontendPidFile) {
    $existingPid = Get-Content $frontendPidFile -ErrorAction SilentlyContinue
    if ($existingPid) {
      $existingProcess = Get-Process -Id $existingPid -ErrorAction SilentlyContinue
      if ($existingProcess) {
        Write-Host "Frontend dev server already running (PID $existingPid)." -ForegroundColor Yellow
        return
      }
    }
    Remove-Item $frontendPidFile -ErrorAction SilentlyContinue
  }

  Write-Step "Starting frontend dev server"
  $process = Start-Process -FilePath "powershell.exe" `
    -ArgumentList "-NoExit", "-Command", "Set-Location '$frontendDir'; npm run dev -- --host 0.0.0.0" `
    -PassThru
  $process.Id | Set-Content $frontendPidFile

  Start-Sleep -Seconds 2
  Write-Host "Frontend started at $viteUrl" -ForegroundColor Green
}

function Start-DockerServices {
  Write-Step "Starting Redis/MySQL with Docker Compose"
  Push-Location $composeDir
  try {
    docker compose up -d
  } finally {
    Pop-Location
  }
}

Assert-Tool $cmake "cmake"
Assert-Tool $gcc "gcc"
Assert-Tool $gxx "g++"

Write-Step "Configuring project"
& $cmake -S $repoRoot -B $buildDir `
  -G "MinGW Makefiles" `
  -DCMAKE_C_COMPILER="$gcc" `
  -DCMAKE_CXX_COMPILER="$gxx" `
  -DCMAKE_BUILD_TYPE=Debug

Write-Step "Building project"
& $cmake --build $buildDir --parallel

if ($WithDocker) {
  Start-DockerServices
}

if (-not $SkipFrontend) {
  Start-Frontend
  if (-not $NoBrowser) {
    Start-Process $viteUrl
  }
}

Write-Step "Running demo client"
& $demoClient

Write-Host ""
Write-Host "Done." -ForegroundColor Green
Write-Host "Demo client has finished."
if (-not $SkipFrontend) {
  Write-Host "Frontend is available at: $viteUrl"
}
if ($WithDocker) {
  Write-Host "Docker services are running from: $composeDir"
}


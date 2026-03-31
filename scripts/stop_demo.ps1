$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path "$PSScriptRoot\.."
$pidDir = Join-Path $repoRoot ".run"
$frontendPidFile = Join-Path $pidDir "frontend.pid"
$composeDir = Join-Path $repoRoot "infra\compose"

if (Test-Path $frontendPidFile) {
  $pid = Get-Content $frontendPidFile -ErrorAction SilentlyContinue
  if ($pid) {
    $process = Get-Process -Id $pid -ErrorAction SilentlyContinue
    if ($process) {
      Stop-Process -Id $pid -Force
      Write-Host "Stopped frontend process PID $pid" -ForegroundColor Green
    }
  }
  Remove-Item $frontendPidFile -ErrorAction SilentlyContinue
}

Push-Location $composeDir
try {
  docker compose down | Out-Null
  Write-Host "Docker services stopped" -ForegroundColor Green
} catch {
  Write-Host "Docker services were not running or docker is unavailable." -ForegroundColor Yellow
} finally {
  Pop-Location
}


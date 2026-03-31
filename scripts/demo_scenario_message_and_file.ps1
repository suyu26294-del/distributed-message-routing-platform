$buildDir = Resolve-Path "$PSScriptRoot\..\build\mingw-debug"
$client = Join-Path $buildDir "demo_client.exe"

if (-not (Test-Path $client)) {
  Write-Error "demo_client.exe not found. Run configure/build first."
  exit 1
}

& $client


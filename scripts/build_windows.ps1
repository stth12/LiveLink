param([string]$BuildDir = "$PSScriptRoot/../build", [string]$JuceDir = "", [string]$CMake = "cmake")
$ErrorActionPreference = 'Stop'
if (-not (Get-Command $CMake -ErrorAction SilentlyContinue)) {
  $candidate = Get-ChildItem 'C:/Program Files/Microsoft Visual Studio' -Filter cmake.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
  if (-not $candidate) { throw 'Install Visual Studio C++ Desktop workload with CMake.' }
  $CMake = $candidate.FullName
}
& $CMake -S "$PSScriptRoot/.." -B $BuildDir -A x64 "-DJUCE_DIR=$JuceDir"
if ($LASTEXITCODE) { throw 'CMake configuration failed' }
& $CMake --build $BuildDir --config Release --parallel 4
if ($LASTEXITCODE) { throw 'Build failed' }
$ctest = Join-Path (Split-Path -Parent (Get-Command $CMake).Source) 'ctest.exe'
& $ctest --test-dir $BuildDir -C Release --output-on-failure
if ($LASTEXITCODE) { throw 'Tests failed' }

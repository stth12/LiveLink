param([string]$BuildDir = "$PSScriptRoot/../build", [switch]$Integration)
$ErrorActionPreference = 'Stop'
& "$BuildDir/Release/LiveLinkTests.exe"
if ($LASTEXITCODE) { throw 'Core tests failed' }
& "$BuildDir/Release/LiveLinkReceiverTests.exe"
if ($LASTEXITCODE) { throw 'Receiver tests failed' }
& "$BuildDir/Release/LiveLinkPluginTests.exe"
if ($LASTEXITCODE) { throw 'Plugin tests failed' }
if ($Integration) {
  $servicePath = (Resolve-Path "$BuildDir/LiveLinkService_artefacts/Release/Live Link Service.exe").Path
  $service = Start-Process -FilePath $servicePath -WindowStyle Hidden -PassThru
  Start-Sleep -Milliseconds 700
  if ($service.HasExited) { throw 'Test process did not start. Close an existing Live Link process first.' }
  try { & "$BuildDir/Release/LiveLinkProbe.exe"; if ($LASTEXITCODE) { throw "Integration test failed ($LASTEXITCODE)" } }
  finally { & $servicePath --stop; $service.WaitForExit(5000) | Out-Null }
}

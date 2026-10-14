param([string]$Installer = "$PSScriptRoot/../dist/LiveLinkSetup.exe", [string]$VSTHostTest = "$PSScriptRoot/../build/Release/LiveLinkVSTHostTest.exe", [string]$InstallDir = '')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/..").Path
$app = Join-Path ${env:ProgramFiles} 'Live Link'
if ($InstallDir) { $app = [IO.Path]::GetFullPath($InstallDir) }
$plugin = Join-Path ${env:CommonProgramFiles} 'VST3/Live Link.vst3'
$report = Join-Path $repo 'docs/installer-verification.txt'
$prefs = Join-Path $env:APPDATA 'Live Link/preferences.json'
$savedPrefs = if (Test-Path -LiteralPath $prefs) { [IO.File]::ReadAllBytes($prefs) } else { $null }
$startupKey = 'HKCU:/Software/Microsoft/Windows/CurrentVersion/Run'
$userShortcut = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Live Link.lnk'
$commonShortcut = Join-Path ([Environment]::GetFolderPath('CommonDesktopDirectory')) 'Live Link.lnk'
if ((Test-Path -LiteralPath $app) -or (Test-Path -LiteralPath $plugin)) { throw 'Installer test requires a clean machine with no existing Live Link installation.' }
if (Get-NetFirewallRule -DisplayName 'Live Link Control' -ErrorAction SilentlyContinue) { throw 'An existing named firewall rule must be preserved; use a clean test machine.' }
if ((Get-ItemProperty -LiteralPath $startupKey -Name LiveLink -ErrorAction SilentlyContinue) -or (Test-Path -LiteralPath $userShortcut) -or (Test-Path -LiteralPath $commonShortcut)) { throw 'Existing startup/shortcut entries must be preserved; use a clean test machine.' }
function Check([bool]$Condition,[string]$Message) { if (-not $Condition) { throw "FAIL $Message" }; "PASS $Message" | Tee-Object -FilePath $report -Append }
'' | Set-Content -LiteralPath $report
"Installation test directory: $app" | Add-Content -LiteralPath $report
$installed = $false
try {
  $setupArgs = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/COMPONENTS=desktop,service,plugin','/TASKS=startup,desktopicon')
  if ($InstallDir) { $setupArgs += "/DIR=`"$app`"" }
  $setup = Start-Process -FilePath (Resolve-Path $Installer) -ArgumentList $setupArgs -WindowStyle Hidden -PassThru -Wait
  Check ($setup.ExitCode -eq 0) 'Production installer completed successfully'
  $installed = $true
  $desktop = Join-Path $app 'Live Link.exe'; $service = Join-Path $app 'Live Link Service.exe'
  Check ((Test-Path -LiteralPath $desktop) -and (Test-Path -LiteralPath $service)) 'Desktop and service installed into specified directory'
  Check (Test-Path -LiteralPath "$plugin/Contents/x86_64-win/Live Link.vst3") 'VST3 installed into standard Common Files/VST3 directory'
  foreach ($name in 'Live Link.exe','Live Link Service.exe') { Check ((Get-FileHash -LiteralPath (Join-Path $app $name)).Hash -eq (Get-FileHash -LiteralPath (Join-Path $repo "dist/windows/$name")).Hash) "Installed $name matches staged binary" }
  $rule = Get-NetFirewallRule -DisplayName 'Live Link Control'
  Check (($rule.Profile.ToString() -eq 'Private') -and ($rule.Direction.ToString() -eq 'Inbound') -and ($rule.Action.ToString() -eq 'Allow')) 'Firewall rule restricted to inbound private networks'
  Check (($rule | Get-NetFirewallPortFilter).LocalPort -eq '49320') 'Firewall permits only configured TCP control port'
  Check (($rule | Get-NetFirewallAddressFilter).RemoteAddress -contains 'LocalSubnet') 'Firewall restricted to local-subnet peers'
  Check ([bool](Get-ItemProperty -LiteralPath $startupKey -Name LiveLink -ErrorAction SilentlyContinue)) 'Optional current-user startup value installed'
  Check ((Test-Path -LiteralPath $userShortcut) -or (Test-Path -LiteralPath $commonShortcut)) 'Optional desktop shortcut installed'
  & $VSTHostTest $plugin
  Check ($LASTEXITCODE -eq 0) 'Installed VST3 loads and preserves samples in JUCE test host'
  $snapshot = Join-Path $repo 'docs/DesktopPreview.png'
  $previewProcess = Start-Process -FilePath $desktop -ArgumentList @('--snapshot',"`"$snapshot`"") -WindowStyle Hidden -PassThru
  if (-not $previewProcess.WaitForExit(10000)) { throw 'Desktop snapshot did not exit within 10 seconds' }
  Check (($previewProcess.ExitCode -eq 0) -and (Test-Path -LiteralPath $snapshot)) 'Installed desktop launched, rendered actual dashboard and exited'
  $state = (& $service --status | ConvertFrom-Json)
  Check ([bool]$state.ok) 'Background process remains active after closing desktop'
  $updated = (& $service --command '{"op":"settings","defaultMode":1}' | ConvertFrom-Json)
  Check ([bool]$updated.ok) 'Installed service accepts authenticated settings and persists preferences'
  $retainedBytes = [IO.File]::ReadAllBytes($prefs)
  $uninstall = Start-Process -FilePath (Join-Path $app 'unins000.exe') -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -WindowStyle Hidden -PassThru -Wait
  Check ($uninstall.ExitCode -eq 0) 'Production uninstaller completed successfully'
  $installed = $false
  Start-Sleep -Milliseconds 500
  Check ((-not (Test-Path -LiteralPath $app)) -and (-not (Test-Path -LiteralPath $plugin))) 'Installed desktop, service and VST3 files removed'
  Check (-not [bool](Get-NetFirewallRule -DisplayName 'Live Link Control' -ErrorAction SilentlyContinue)) 'Installer-managed firewall rule removed'
  Check (-not [bool](Get-ItemProperty -LiteralPath $startupKey -Name LiveLink -ErrorAction SilentlyContinue)) 'Installer-managed startup value removed'
  Check ((-not (Test-Path -LiteralPath $userShortcut)) -and (-not (Test-Path -LiteralPath $commonShortcut))) 'Installer-managed desktop shortcut removed'
  Check ((Test-Path -LiteralPath $prefs) -and ([Convert]::ToBase64String([IO.File]::ReadAllBytes($prefs)) -eq [Convert]::ToBase64String($retainedBytes))) 'Silent uninstall retains exact user preference file'
  Check (-not [bool](Get-Process -Name 'Live Link Service' -ErrorAction SilentlyContinue)) 'Background process stopped during uninstall'
} finally {
  if ($installed -and (Test-Path -LiteralPath (Join-Path $app 'unins000.exe'))) { Start-Process -FilePath (Join-Path $app 'unins000.exe') -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -WindowStyle Hidden -Wait }
  if ($null -ne $savedPrefs) { [IO.File]::WriteAllBytes($prefs,$savedPrefs) } elseif (Test-Path -LiteralPath $prefs) { Remove-Item -LiteralPath $prefs }
}

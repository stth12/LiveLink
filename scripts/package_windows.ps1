param([string]$BuildDir = "$PSScriptRoot/../build", [string]$ISCC = "ISCC.exe")
$ErrorActionPreference = 'Stop'
$repository = (Resolve-Path "$PSScriptRoot/..").Path
$stage = Join-Path $repository 'dist/windows'
New-Item -ItemType Directory -Path $stage -Force | Out-Null
Copy-Item -LiteralPath "$BuildDir/LiveLinkDesktop_artefacts/Release/Live Link.exe" -Destination $stage
Copy-Item -LiteralPath "$BuildDir/LiveLinkService_artefacts/Release/Live Link Service.exe" -Destination $stage
Copy-Item -LiteralPath "$BuildDir/LiveLinkPlugin_artefacts/Release/VST3/Live Link.vst3" -Destination $stage -Recurse -Force
Copy-Item -LiteralPath "$repository/LICENSE","$repository/THIRD_PARTY_NOTICES.md" -Destination $stage
Copy-Item -LiteralPath "$repository/COPYING","$repository/third_party/JUCE-LICENSE.md" -Destination $stage
& $ISCC "$repository/installer/windows/LiveLink.iss"
if ($LASTEXITCODE) { throw 'Installer compilation failed' }

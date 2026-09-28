param(
  [string]$PackageRoot = "$PSScriptRoot\10bit-broadcast-dock"
)
$ErrorActionPreference = 'Stop'

$obs = Get-Process obs64 -ErrorAction SilentlyContinue
if ($obs) {
  Write-Error 'OBS Studio is running. Close OBS before installing 10BIT Dock.'
  exit 2
}

$src = Resolve-Path $PackageRoot
$dst = Join-Path $env:ProgramData 'obs-studio\plugins\10bit-broadcast-dock'
New-Item -ItemType Directory -Force $dst | Out-Null
Copy-Item -Path (Join-Path $src '*') -Destination $dst -Recurse -Force

$dll = Join-Path $dst 'bin\64bit\10bit-broadcast-dock.dll'
if (!(Test-Path $dll)) { throw "Plugin DLL missing after install: $dll" }
Write-Host "Installed: $dll"
Write-Host 'Restart OBS Studio, then open Docks > 10BIT Broadcast.'

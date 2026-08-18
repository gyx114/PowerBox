# =============================================================
# PowerBox one-click packaging script (Inno Setup 6)
# - Prompts for a version (e.g. v3.0.0) and bakes it into the installer.
# - Packages files from <repo>\x64\Release and outputs PowerBoxSetup.exe there.
# - Paths are relative to this script, so place setup.iss + build.ps1 together
#   at the repos root (then "x64\Release" = the folder to package).
# ASCII-only on purpose (no encoding issues).
#     powershell -ExecutionPolicy Bypass -File build.ps1
# =============================================================
$ErrorActionPreference = 'Stop'

$ScriptDir = $PSScriptRoot             # dir containing this script
$BuildOut  = Join-Path $ScriptDir 'x64\Release'
$IssPath   = Join-Path $ScriptDir 'setup.iss'
$OutFile   = Join-Path $BuildOut 'PowerBoxSetup.exe'

# --- 0) locate ISCC -------------------------------------------
$ISCC = $env:ISCC_PATH
if (!$ISCC) {
    $cmd = Get-Command 'ISCC.exe' -ErrorAction SilentlyContinue
    if ($cmd) { $ISCC = $cmd.Source }
}
if (!$ISCC) {
    $candidates = @(
        'D:\Inno Setup 6\ISCC.exe',
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
    )
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            $ISCC = $candidate
            break
        }
    }
}
if (!$ISCC) {
    Write-Host 'ISCC.exe not found. Install Inno Setup 6 or set $env:ISCC_PATH.' -ForegroundColor Red
    exit 1
}
if (!(Test-Path $IssPath)) {
    Write-Host "Not found: $IssPath  (setup.iss must sit next to build.ps1)" -ForegroundColor Red
    exit 1
}

# --- 1) sanity: only package complete build output -------------
$required = @(
    (Join-Path $BuildOut 'PowerBox.exe'),
    (Join-Path $BuildOut 'WebView2Loader.dll'),
    (Join-Path $BuildOut 'lang'),
    (Join-Path $BuildOut 'res\markdown')
)
foreach ($path in $required) {
    if (!(Test-Path $path)) {
        Write-Host "Missing required packaging input: $path" -ForegroundColor Red
        Write-Host 'Build the project first and copy the files to the root x64\Release folder.' -ForegroundColor Yellow
        exit 1
    }
}

# --- 2) ask version (must start with 'v', e.g. v3.0.0) ---------
$verDisp = ''
while ($true) {
    $entered = Read-Host 'Please type a release version (start with v, e.g. v3.0.0)'
    $entered = $entered.Trim()
    if ($entered -match '^[vV]\d+(\.\d+){1,3}$') {
        $verDisp = $entered   # keep the display form "v3.0.0"
        break
    }
    Write-Host "Invalid format: '$entered'. Expected something like v3.0.0 (must start with v)." -ForegroundColor Yellow
}
# Inno's AppVersion / VersionInfoVersion need the numeric dotted part.
$ver = $verDisp -replace '^[vV]', ''
Write-Host "Packaging version: $verDisp  (numeric: $ver)" -ForegroundColor Cyan

# --- 3) compile installer into the same Release folder ---------
Write-Host 'Running ISCC...' -ForegroundColor Cyan
& $ISCC "/DAppVersion=$ver" "/O$BuildOut" $IssPath
if ($LASTEXITCODE -ne 0) {
    Write-Host "ISCC failed (exit code $LASTEXITCODE)." -ForegroundColor Red
    exit $LASTEXITCODE
}

if (Test-Path $OutFile) {
    Write-Host "Done: $OutFile" -ForegroundColor Green
} else {
    Write-Host 'Build finished but PowerBoxSetup.exe was not found at the expected path.' -ForegroundColor Red
    exit 1
}

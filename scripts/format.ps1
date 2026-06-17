<#
  format.ps1 -- bms-app source formatting (uses .clang-format / Zephyr style)
  ----------------------------------------------------------------------------
  Single source of truth for "which files get formatted". Both local use and CI
  call this script so behaviour stays identical.

  Scope: project code only (app / drivers / tests). Third-party trees
  (lib / CMSIS / zephyr) are never touched.

  Usage:
    # Check only (no writes; non-zero exit if any file needs formatting -> CI gate)
    powershell -ExecutionPolicy Bypass -File bms-app\scripts\format.ps1 -Check
    # Fix (format all files in place)
    powershell -ExecutionPolicy Bypass -File bms-app\scripts\format.ps1
#>
[CmdletBinding()]
param(
    [switch]$Check
)

$ErrorActionPreference = "Stop"

# bms-app repo root = parent of this script's directory
$Root = Split-Path -Parent $PSScriptRoot

# Prefer the venv clang-format, fall back to PATH
$venvCf = "D:\__00_WorkSpace\__06_Study\bms-workspace\.venv\Scripts\clang-format.exe"
$ClangFormat = if (Test-Path $venvCf) { $venvCf } else { "clang-format" }

# Directories that participate in formatting (project-owned code)
$dirs = @("app", "drivers", "tests") | ForEach-Object { Join-Path $Root $_ } |
        Where-Object { Test-Path $_ }

# Collect .c / .h files, excluding any build output
$files = $dirs | ForEach-Object {
    Get-ChildItem $_ -Recurse -Include *.c, *.h -File
} | Where-Object { $_.FullName -notmatch '\\build\\' } | Select-Object -ExpandProperty FullName

if (-not $files) { Write-Host "No files to format."; exit 0 }

$mode = if ($Check) { "check" } else { "fix" }
Write-Host ("clang-format: {0}" -f $ClangFormat)
Write-Host ("files: {0}  mode: {1}" -f $files.Count, $mode) -ForegroundColor Cyan

if ($Check) {
    # --dry-run --Werror: non-zero exit if any file would change
    & $ClangFormat --dry-run --Werror --style=file $files
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAIL: some files are not formatted (see diff above). Run without -Check to fix." -ForegroundColor Red
        exit 1
    }
    Write-Host "OK: all files conform to the format style." -ForegroundColor Green
} else {
    & $ClangFormat -i --style=file $files
    Write-Host ("OK: formatted {0} files in place." -f $files.Count) -ForegroundColor Green
}

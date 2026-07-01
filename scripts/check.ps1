<#
  check.ps1 -- local mirror of the CI gates (.github/workflows/ci.yml).
  ----------------------------------------------------------------------------
  Run this before opening a PR so you do not push something that turns the PR
  red. It reproduces the CI gates locally, in the same scopes:
      format -> build (mps2/an386) -> build (native_sim) -> twister
             -> sca-gcc -> clang-tidy

  Prerequisites: run from an ACTIVATED Zephyr venv so `west` is on PATH.
  Optional tools (clang-tidy) are detected; if missing, that gate is SKIPped
  (reported, not failed). SCA + clang-tidy do clean (-p always) builds, so a
  full run takes several minutes -- use -Fast to skip those two heavy gates.

  Usage:
    powershell -ExecutionPolicy Bypass -File scripts\check.ps1          # all gates
    powershell -ExecutionPolicy Bypass -File scripts\check.ps1 -Fast    # format + build + test only
#>
[CmdletBinding()]
param([switch]$Fast)

$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

function Have($cmd) { $null -ne (Get-Command $cmd -ErrorAction SilentlyContinue) }

# Ordered summary of gate -> result (PASS / FAIL / SKIP).
$summary = New-Object System.Collections.Specialized.OrderedDictionary

# Run a gate body that returns 'PASS', 'FAIL', or 'SKIP'. Any throw => FAIL.
function Gate([string]$name, [scriptblock]$body) {
    Write-Host ""
    Write-Host ("=== {0} ===" -f $name) -ForegroundColor Cyan
    $global:LASTEXITCODE = 0
    $res = "FAIL"
    try { $res = & $body } catch { Write-Host $_.Exception.Message -ForegroundColor Red; $res = "FAIL" }
    if (@("PASS","FAIL","SKIP","WARN") -notcontains $res) { $res = "FAIL" }
    $summary[$name] = $res
    $color = @{ PASS = "Green"; FAIL = "Red"; SKIP = "Yellow"; WARN = "Yellow" }[$res]
    Write-Host ("{0}: {1}" -f $res, $name) -ForegroundColor $color
}

# --- Gate 1: formatting (no west needed) ----------------------------------
Gate "format" {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $Root "scripts\format.ps1") -Check
    if ($LASTEXITCODE -eq 0) { "PASS" } else { "FAIL" }
}

# --- Gate 1b: file-header hygiene (C:002 SPDX + C:004 doxygen; no west needed).
Gate "file-headers" {
    $py = Get-Command python -ErrorAction SilentlyContinue
    if (-not $py) { $py = Get-Command python3 -ErrorAction SilentlyContinue }
    if (-not $py) { Write-Host "python not found; skipping." -ForegroundColor Yellow; return "SKIP" }
    & $py.Source (Join-Path $Root "scripts\check-file-headers.py")
    if ($LASTEXITCODE -eq 0) { "PASS" } else { "FAIL" }
}

if (-not (Have "west")) {
    Write-Host ""
    Write-Host "west not found on PATH -- activate the Zephyr venv to run build/test/SCA/tidy gates." -ForegroundColor Yellow
    Write-Host "  e.g.  ..\.venv\Scripts\Activate.ps1" -ForegroundColor Yellow
    foreach ($g in "build (mps2/an386)", "build (native_sim)", "twister (mps2/an386)") { $summary[$g] = "SKIP" }
    if (-not $Fast) { foreach ($g in "sca-gcc", "clang-tidy") { $summary[$g] = "SKIP" } }
} else {
    # --- Gate 2: build both compilable targets (clean) --------------------
    Gate "build (mps2/an386)" {
        # Export compile_commands.json here -- it builds on Windows (unlike
        # native_sim) and feeds cppcheck's accurate PROJECT mode below.
        west build -b mps2/an386 app -d build\check-an386 -p always -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        if ($LASTEXITCODE -eq 0) { "PASS" } else { "FAIL" }
    }
    Gate "build (native_sim)" {
        west build -b native_sim app -d build\check-nsim -p always
        if ($LASTEXITCODE -eq 0) { "PASS" } else { "FAIL" }
    }

    # --- Gate 3: unit tests. CI uses native_sim; locally QEMU/mps2 is the
    #     reliable route on Windows (native_sim needs a host toolchain). Point
    #     twister at the SDK QEMU if QEMU_BIN_PATH is not already set. ---------
    Gate "twister (mps2/an386)" {
        $sdkQemu = "D:\zephyr-sdk\zephyr-sdk-1.0.1\hosttools\qemu"
        if ((Test-Path $sdkQemu) -and -not $env:QEMU_BIN_PATH) { $env:QEMU_BIN_PATH = $sdkQemu }
        west twister -T tests -p mps2/an386 -c --inline-logs
        if ($LASTEXITCODE -eq 0) { "PASS" } else { "FAIL" }
    }

    if ($Fast) {
        Write-Host ""
        Write-Host "-Fast: skipping sca-gcc and clang-tidy (run without -Fast for the full mirror)." -ForegroundColor Yellow
        $summary["sca-gcc"]    = "SKIP"
        $summary["clang-tidy"] = "SKIP"
    } else {
        # --- Gate 4: SCA (gcc -fanalyzer), gated to app code by sca-check.sh.
        Gate "sca-gcc" {
            west build -b mps2/an386 app -d build\check-sca -p always -- -DZEPHYR_SCA_VARIANT=gcc 2>&1 |
                Out-String -Stream | Tee-Object -FilePath build\sca-build.log | Out-Null
            if (-not (Have "bash")) { Write-Host "bash not found; cannot run sca-check.sh." -ForegroundColor Yellow; return "SKIP" }
            bash scripts/sca-check.sh build/sca-build.log
            if ($LASTEXITCODE -eq 0) { "PASS" } else { "FAIL" }
        }

        # --- Gate 5: clang-tidy (CERT/readability). Needs clang-tidy locally. -
        Gate "clang-tidy" {
            if (-not (Have "clang-tidy")) { Write-Host "clang-tidy not installed; skipping." -ForegroundColor Yellow; return "SKIP" }
            # clang-tidy parity needs native_sim (host flags). native_sim does NOT
            # configure on Windows, and a locally-newer clang-tidy diverges from
            # CI's. So clang-tidy is CI(Linux)-authoritative; SKIP if the build
            # cannot be produced here. For local parity run check in WSL2.
            west build -b native_sim app -d build\check-tidy -p always -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
            if ($LASTEXITCODE -ne 0) {
                Write-Host "native_sim build unavailable (expected on Windows); clang-tidy is CI/WSL2-authoritative -> SKIP." -ForegroundColor Yellow
                return "SKIP"
            }
            # Strip gcc-only flags clang rejects (same set as CI's sed step).
            $ccPath = "build\check-tidy\compile_commands.json"
            $cc = Get-Content $ccPath -Raw
            $cc = $cc -replace '-fno-(reorder-functions|freestanding|defer-pop|printf-return-value|reorder-blocks-and-partition)', ''
            $cc = $cc -replace '--param=[^ "]*', ''
            [System.IO.File]::WriteAllText((Resolve-Path $ccPath).Path, $cc)
            $cfiles = Get-ChildItem app\src -Recurse -Filter *.c -File | Select-Object -ExpandProperty FullName
            if (-not $cfiles) { Write-Host "no app/src/*.c files." -ForegroundColor Yellow; return "SKIP" }
            clang-tidy -p build\check-tidy $cfiles
            if ($LASTEXITCODE -eq 0) { "PASS" } else { "FAIL" }
        }
    }
}

# --- Gate 6: cppcheck + MISRA (WARN-ONLY; no build needed). Runs regardless of
#     west since cppcheck analyzes sources directly. Findings -> WARN, not FAIL.
if (-not $Fast) {
    Gate "cppcheck+MISRA (warn)" {
        # No 'Have cppcheck' precheck: cppcheck-run.sh resolves cppcheck itself
        # (PATH or C:/Program Files/Cppcheck), which PowerShell's PATH may miss.
        if (-not (Have "bash")) { Write-Host "bash not found; cannot run cppcheck-run.sh." -ForegroundColor Yellow; return "SKIP" }
        # Prefer PROJECT mode using the mps2 build gate's compile db (accurate,
        # real -I/-D -> few false positives; mps2 builds on Windows, unlike the
        # native_sim db clang-tidy uses). Fall back to standalone if absent.
        $cc = "build/check-an386/compile_commands.json"
        if (Test-Path $cc) {
            Write-Host "project mode (accurate) via $cc" -ForegroundColor DarkGray
            $out = (bash scripts/cppcheck-run.sh --project $cc 2>&1 | Out-String)
        } else {
            Write-Host "no compile db -> standalone mode (rough; Zephyr headers unresolved)." -ForegroundColor Yellow
            $files = Get-ChildItem app\src -Recurse -Filter *.c -File | Select-Object -ExpandProperty FullName
            if (-not $files) { Write-Host "no app/src/*.c files." -ForegroundColor Yellow; return "SKIP" }
            $rel = $files | ForEach-Object { ($_ -replace [regex]::Escape("$Root\"), "") -replace '\\','/' }
            $out = (bash scripts/cppcheck-run.sh @rel 2>&1 | Out-String)
        }
        Write-Host $out
        if     ($out -match 'cppcheck not found')        { "SKIP" }
        elseif ($out -match 'cppcheck-run: 0 findings')  { "PASS" }
        elseif ($out -match 'finding\(s\)')              { "WARN" }
        else                                             { "PASS" }
    }
} else {
    $summary["cppcheck+MISRA (warn)"] = "SKIP"
}

# --- Summary --------------------------------------------------------------
Write-Host ""
Write-Host "==================== summary ====================" -ForegroundColor Cyan
$anyFail = $false
foreach ($k in $summary.Keys) {
    $v = $summary[$k]
    if ($v -eq "FAIL") { $anyFail = $true }
    $color = @{ PASS = "Green"; FAIL = "Red"; SKIP = "Yellow"; WARN = "Yellow" }[$v]
    Write-Host ("  {0,-22} {1}" -f $k, $v) -ForegroundColor $color
}
Write-Host "================================================="
if ($anyFail) {
    Write-Host "RESULT: FAIL -- fix the gates above before opening a PR." -ForegroundColor Red
    exit 1
}
Write-Host "RESULT: OK -- safe to push / open a PR (SKIP = gate not run locally; CI will run it)." -ForegroundColor Green
exit 0

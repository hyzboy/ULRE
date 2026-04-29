<#
.SYNOPSIS
    CI log blacklist scanner for ULRE ShaderGen / Vulkan pipeline diagnostics.

.DESCRIPTION
    Scans example-run log files for known fatal error patterns introduced in
    Step 3.5 of VertexInputFormat_plan.md.  Any hit causes the script to exit
    with code 1, which fails the CI pipeline.

    Blacklisted patterns (any match = failure):
      1. "VariantRegistry lookup failed"
             Factory-layer QueryVariant miss after RouteKey should have resolved.
      2. "factory dispatch failed"
             MaterialLibrary::CreateMaterialCreateInfo could not dispatch a factory.
      3. "[PipelineBuild.VertexInputDiff]"
             GeometryVIL vs MaterialDefaultVIL mismatch detected at pipeline build.
      4. "Primitive mismatch: cfg->prim="
             cfg->prim disagrees with def.primitive_type inside CreateFromFixedDef3D.

.PARAMETER LogDir
    Directory to scan recursively for *.log files.
    Defaults to <repo-root>/logs if not specified.

.PARAMETER LogFiles
    One or more explicit log file paths.  Takes precedence over -LogDir.

.PARAMETER RepoRoot
    Root of the ULRE repository.  Used to resolve the default log directory.
    Defaults to the directory two levels above this script (scripts/ci/ -> root).

.EXAMPLE
    # Scan default logs/ directory
    .\scripts\ci\check_blacklist.ps1

    # Scan a specific directory
    .\scripts\ci\check_blacklist.ps1 -LogDir E:\ULRE\logs\examples

    # Scan explicit files
    .\scripts\ci\check_blacklist.ps1 -LogFiles draw_triangle.log,grid2d.log
#>

[CmdletBinding()]
param(
    [string]   $LogDir   = "",
    [string[]] $LogFiles = @(),
    [string]   $RepoRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Resolve repo root
# ---------------------------------------------------------------------------
if (-not $RepoRoot) {
    # This script lives at <repo>/scripts/ci/check_blacklist.ps1
    # Join-Path with 3 arguments requires PS 7+; use nested calls for PS 5.1 compat.
    $RepoRoot = (Resolve-Path (Join-Path (Join-Path $PSScriptRoot "..") "..")).Path
}

# ---------------------------------------------------------------------------
# Blacklist patterns
# Each entry: @{ Pattern = <regex string>; Description = <human label> }
# All comparisons are case-sensitive to avoid false positives.
# ---------------------------------------------------------------------------
$Blacklist = @(
    @{
        Pattern     = 'VariantRegistry lookup failed'
        Description = 'Factory-layer VariantRegistry miss after RouteKey resolution'
    },
    @{
        Pattern     = 'factory dispatch failed'
        Description = 'MaterialLibrary::CreateMaterialCreateInfo factory dispatch failure'
    },
    @{
        Pattern     = '\[PipelineBuild\.VertexInputDiff\]'
        Description = 'GeometryVIL vs MaterialDefaultVIL mismatch at pipeline build'
    },
    @{
        Pattern     = 'Primitive mismatch: cfg->prim='
        Description = 'cfg->prim disagrees with def.primitive_type in CreateFromFixedDef3D'
    }
)

# ---------------------------------------------------------------------------
# Collect files to scan
# ---------------------------------------------------------------------------
[System.Collections.Generic.List[string]] $filesToScan = @()

if ($LogFiles.Count -gt 0) {
    foreach ($f in $LogFiles) {
        $resolved = Resolve-Path $f -ErrorAction SilentlyContinue
        if ($resolved) {
            $filesToScan.Add($resolved.Path)
        } else {
            Write-Warning "check_blacklist: file not found, skipping: $f"
        }
    }
} else {
    if (-not $LogDir) {
        $LogDir = Join-Path $RepoRoot "logs"
    }
    if (-not (Test-Path $LogDir)) {
        Write-Host "check_blacklist: log directory not found, nothing to scan: $LogDir"
        Write-Host "check_blacklist: PASS (no logs present)"
        exit 0
    }
    Get-ChildItem -Path $LogDir -Recurse -File -Filter "*.log" | ForEach-Object {
        $filesToScan.Add($_.FullName)
    }
}

if ($filesToScan.Count -eq 0) {
    Write-Host "check_blacklist: no log files found to scan."
    Write-Host "check_blacklist: PASS (no logs present)"
    exit 0
}

Write-Host "check_blacklist: scanning $($filesToScan.Count) log file(s)..."

# ---------------------------------------------------------------------------
# Scan
# ---------------------------------------------------------------------------
$totalHits = 0

foreach ($file in $filesToScan) {
    $lineNumber = 0
    $content    = Get-Content -Path $file -Encoding UTF8 -ErrorAction SilentlyContinue
    if ($null -eq $content) { continue }

    foreach ($line in $content) {
        $lineNumber++
        foreach ($entry in $Blacklist) {
            if ($line -cmatch $entry.Pattern) {
                Write-Host ""
                Write-Host "  [BLACKLIST HIT] $($entry.Description)"
                Write-Host "    File   : $file"
                Write-Host "    Line   : $lineNumber"
                Write-Host "    Content: $line"
                $totalHits++
            }
        }
    }
}

# ---------------------------------------------------------------------------
# Result
# ---------------------------------------------------------------------------
Write-Host ""
if ($totalHits -gt 0) {
    Write-Host "check_blacklist: FAIL — $totalHits blacklisted line(s) found." -ForegroundColor Red
    exit 1
} else {
    Write-Host "check_blacklist: PASS — 0 blacklisted lines across $($filesToScan.Count) file(s)." -ForegroundColor Green
    exit 0
}

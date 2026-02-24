#!/usr/bin/env pwsh

# Simple compile test for RenderSystemGroup
$sourceFiles = @(
    "e:\ULRE\src\ecs\core\RenderSystemGroup.cpp",
    "e:\ULRE\src\ecs\core\RenderGraph.cpp"
)

$includeDir = "e:\ULRE\inc"
$outputDir = "e:\ULRE\build\obj"

# Create output directory
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

# Compile command
$cl = "cl.exe"
$args = @(
    "/I$includeDir",
    "/c",
    "/Fo$outputDir\",
    "/W4",
    "/std:c++latest",
    "/EHsc"
) + $sourceFiles

Write-Host "Compiling RenderSystemGroup and RenderGraph..."
Write-Host "Command: $cl $($ args -join ' ')"

& $cl @args

if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Compilation successful"
} else {
    Write-Host "✗ Compilation failed with exit code $LASTEXITCODE"
}

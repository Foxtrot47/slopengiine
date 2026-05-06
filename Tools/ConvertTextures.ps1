# ConvertTextures.ps1 — Converts non-DDS textures to GPU-compressed DDS format
# Converts source textures to a build output directory, preserving directory structure.
# Skips files that already have an up-to-date .dds in the output directory.
#
# Usage: powershell -File Tools/ConvertTextures.ps1 -SourceDir <path> -OutputDir <path> [-ToolPath <path>]

param(
    [string]$SourceDir = "Assets",
    [string]$OutputDir = "",  # required: build output directory for DDS files
    [string]$ToolPath  = "Tools/texconv.exe"
)

$ErrorActionPreference = "Stop"

if ($OutputDir -eq "") {
    Write-Error "OutputDir is required (specify build output directory for DDS files)"
    exit 1
}

if (-not (Test-Path $ToolPath)) {
    Write-Error "texconv.exe not found at: $ToolPath"
    exit 1
}

$ToolPath = Resolve-Path $ToolPath

# Extensions to convert (case-insensitive)
$convertExtensions = @(".png", ".jpg", ".jpeg", ".tga", ".bmp", ".exr", ".hdr")

# Determine format by filename pattern
function Get-DdsFormat($filename) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($filename).ToLower()

    # HDR/EXR → BC6H
    $ext = [System.IO.Path]::GetExtension($filename).ToLower()
    if ($ext -eq ".exr" -or $ext -eq ".hdr") { return "BC6H_UF16" }

    # Normal maps → BC3_UNORM (preserves XY quality better than BC5 for our pipeline)
    if ($name -match "normal|_nrm|_n$") { return "BC3_UNORM" }

    # Single-channel maps → BC4_UNORM
    if ($name -match "roughness|_rough") { return "BC4_UNORM" }
    if ($name -match "metallic|_metal") { return "BC4_UNORM" }
    if ($name -match "opacity|_alpha|_mask") { return "BC4_UNORM" }
    if ($name -match "specular|_spec") { return "BC4_UNORM" }
    if ($name -match "ao|_occlusion|ambientocclusion") { return "BC4_UNORM" }

    # BaseColor, Emissive, or anything else → BC3_UNORM (RGBA)
    return "BC3_UNORM"
}

# Find all convertible textures recursively
$sourceFullPath = Resolve-Path $SourceDir
$outputFullPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDir)
$textures = Get-ChildItem -Path $sourceFullPath -Recurse -File | Where-Object {
    $convertExtensions -contains $_.Extension.ToLower()
}

if ($textures.Count -eq 0) {
    Write-Host "No textures to convert."
    exit 0
}

Write-Host "Converting textures from: $sourceFullPath"
Write-Host "Output directory: $outputFullPath"
Write-Host "Found $($textures.Count) texture(s) to check..."
$converted = 0
$skipped = 0

foreach ($tex in $textures) {
    # Compute relative path from source root
    $relativePath = $tex.FullName.Substring($sourceFullPath.Path.Length + 1)
    $outDir = Join-Path $outputFullPath (Split-Path $relativePath -Parent)

    $ddsName = [System.IO.Path]::GetFileNameWithoutExtension($tex.Name) + ".dds"
    $ddsPath = Join-Path $outDir $ddsName

    # Skip if DDS exists and is newer than source
    if (Test-Path $ddsPath) {
        $ddsTime = (Get-Item $ddsPath).LastWriteTime
        if ($ddsTime -ge $tex.LastWriteTime) {
            $skipped++
            continue
        }
    }

    # Ensure output directory exists
    if (-not (Test-Path $outDir)) {
        New-Item -ItemType Directory -Path $outDir -Force | Out-Null
    }

    $format = Get-DdsFormat $tex.Name
    $args = @(
        "`"$($tex.FullName)`"",
        "-f", $format,
        "-m", "0",       # generate all mip levels
        "-o", "`"$outDir`"",
        "-y"             # overwrite existing
    )

    Write-Host "  [$format] $($tex.Name)"
    $process = Start-Process -FilePath $ToolPath -ArgumentList $args -NoNewWindow -Wait -PassThru
    if ($process.ExitCode -ne 0) {
        Write-Warning "Failed to convert: $($tex.Name)"
    } else {
        $converted++
    }
}

Write-Host ""
Write-Host "Done: $converted converted, $skipped skipped (up-to-date)."

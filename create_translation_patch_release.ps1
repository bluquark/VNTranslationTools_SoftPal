# create_translation_patch_release.ps1
# Run from the game directory (after copying the tools release into it and running VNTextPatch).
# Creates a release zip to distribute to players with the minimum files needed (patched pac files, fonts, and proxy dll).

$ErrorActionPreference = "Stop"

$unipack = ".\util\unipack.exe"
$png2pgd = ".\util\png2pgd_ge.exe"

# Verify utilities exist
if (-not (Test-Path $unipack)) { throw "unipack.exe not found in util\" }
if (-not (Test-Path $png2pgd)) { throw "png2pgd_ge.exe not found in util\" }

# Verify base files exist
foreach ($f in @("data.pac", "winmm.dll", "VNTranslationToolsConstants.json")) {
    if (-not (Test-Path $f)) { throw "$f not found in current directory" }
}
if (-not (Get-ChildItem -Filter "*.ttf" -ErrorAction SilentlyContinue)) {
    throw "No .ttf font files found in current directory"
}
if (-not (Test-Path "data\TEXT.DAT")) { throw "data\TEXT.DAT not found" }
if (-not (Test-Path "data\script.src")) { throw "data\script.src not found" }
$pngFiles = Get-ChildItem -Path "data" -Filter "*.png" -ErrorAction SilentlyContinue

# 1) Create release directory and tmp
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$release = "translation_patch_release_$timestamp"
$tmpDir = Join-Path $release "tmp"
New-Item -ItemType Directory -Path $tmpDir | Out-Null
Write-Host "Created $release"

# 2) Copy all pac files into tmp/
$pacFiles = Get-ChildItem -Filter "*.pac"
if (-not $pacFiles) { throw "No .pac files found in current directory" }
foreach ($pac in $pacFiles) {
    Copy-Item $pac.FullName $tmpDir
}
Write-Host "Copied $($pacFiles.Count) pac files to tmp/"

# 3) Convert data/*.png to PGD and insert into pac archives
$insertedIn = @{}  # track which pacs had successful insertions

if ($pngFiles) {
    $pgdDir = Join-Path $tmpDir "pgd"
    New-Item -ItemType Directory -Path $pgdDir | Out-Null

    foreach ($png in $pngFiles) {
        $outPgd = Join-Path $pgdDir ($png.BaseName + ".PGD")
        Write-Host "Converting $($png.Name) -> PGD"
        & $png2pgd -m 3 $png.FullName $outPgd
        if ($LASTEXITCODE -ne 0) { throw "png2pgd_ge failed on $($png.Name)" }
    }

    # Try inserting PGD files into each pac
    # unipack prints "update archive with <name>" for each file it replaces
    $pgdNames = (Get-ChildItem -Path $pgdDir -Filter "*.PGD") | ForEach-Object { $_.Name }
    $pgdInserted = @{}  # track which PGDs were successfully inserted

    foreach ($pac in Get-ChildItem -Path $tmpDir -Filter "*.pac") {
        $output = & $unipack $pac.FullName $pgdDir 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) { throw "unipack failed on $($pac.Name)" }

        $hadUpdate = $false
        foreach ($pgdName in $pgdNames) {
            $baseName = [System.IO.Path]::GetFileNameWithoutExtension($pgdName)
            if ($output -match "update archive with $baseName") {
                $hadUpdate = $true
                $pgdInserted[$pgdName] = $true
            }
        }

        $newPac = "$($pac.FullName).new"
        if ($hadUpdate) {
            Move-Item $newPac $pac.FullName -Force
            $insertedIn[$pac.Name] = $true
            Write-Host "Inserted PGD files into $($pac.Name)"
        } elseif (Test-Path $newPac) {
            Remove-Item $newPac
        }
    }

    # Error out if any PGD was not inserted into any pac
    $missing = $pgdNames | Where-Object { -not $pgdInserted[$_] }
    if ($missing) {
        throw "The following PGD files were not found in any pac archive: $($missing -join ', ')"
    }
}

# 5) Insert data/TEXT.DAT and data/script.src into data.pac (in tmp)
$dataPac = Join-Path $tmpDir "data.pac"
Write-Host "Inserting TEXT.DAT and script.src into data.pac"
& $unipack $dataPac "data"
if ($LASTEXITCODE -ne 0) { throw "unipack failed on data.pac" }
Move-Item "$dataPac.new" $dataPac -Force

# 6) Copy required files into release directory
Copy-Item "winmm.dll" $release
Copy-Item "VNTranslationToolsConstants.json" $release
Get-ChildItem -Filter "*.ttf" | ForEach-Object {
    Copy-Item $_.FullName $release
}
# Copy data.pac (always needed) and any pac where PGDs were inserted
Copy-Item $dataPac $release
foreach ($pacName in $insertedIn.Keys) {
    if ($pacName -ne "data.pac") {
        Copy-Item (Join-Path $tmpDir $pacName) $release
    }
}
Write-Host "Copied patched files to release"

# 7) Create zip (contents at root level, exclude tmp/)
$zipName = "$release.zip"
Write-Host "Creating $zipName"
$filesToZip = Get-ChildItem -Path $release | Where-Object { $_.Name -ne "tmp" }
Compress-Archive -Path $filesToZip.FullName -DestinationPath $zipName
Write-Host "Done: $zipName"

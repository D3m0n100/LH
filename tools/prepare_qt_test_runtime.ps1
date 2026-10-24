param(
    [string]$QtRoot = 'C:\Qt\5.15.2\mingw81_64',
    [string]$RuntimeDirectory = (Join-Path $PSScriptRoot '..\build_current_mingw\bin')
)
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$destination = [IO.Path]::GetFullPath($RuntimeDirectory)
if (-not $destination.StartsWith($workspace + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Qt test runtime destination must stay inside this workspace.'
}
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Get-ChildItem -LiteralPath (Join-Path $QtRoot 'bin') -Filter '*.dll' | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $destination -Force
}
foreach ($category in @('platforms','sqldrivers','imageformats','styles','iconengines','bearer','canbus')) {
    $source = Join-Path $QtRoot ('plugins\' + $category)
    if (-not (Test-Path -LiteralPath $source)) { continue }
    $target = Join-Path $destination ('plugins\' + $category)
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Get-ChildItem -LiteralPath $source -Filter '*.dll' | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $target -Force
    }
}
Set-Content -LiteralPath (Join-Path $destination 'qt.conf') -Encoding ascii -Value "[Paths]`nPrefix=.`nPlugins=plugins`n"
Write-Output "Prepared matched Qt runtime: $destination"
Write-Output 'Run GUI acceptance with QT_QPA_PLATFORM=windows; Windows offscreen QMessageBox is not a supported test environment.'

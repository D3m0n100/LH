param([string]$Python = "python", [switch]$Offline)
$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
$compiler = Join-Path $repository "third_party/custom_dsp_language/compile"
$environment = Join-Path $compiler "venv"
$interpreter = Join-Path $environment "Scripts/python.exe"
if (Test-Path -LiteralPath $interpreter) {
    & $interpreter -c "import sys; assert sys.version_info >= (3,8)"
    $healthy = $LASTEXITCODE -eq 0
} else { $healthy = $false }
if (-not $healthy) {
    if (Test-Path -LiteralPath $environment) {
        # Retain the old environment for recovery; never silently delete user-installed packages.
        $resolved = [IO.Path]::GetFullPath($environment)
        $expected = [IO.Path]::GetFullPath((Join-Path $compiler "venv"))
        if ($resolved -ne $expected) { throw "Unexpected compiler environment path" }
        $backup = Join-Path $compiler ("venv.backup-" + [DateTime]::UtcNow.ToString("yyyyMMddHHmmssffff"))
        Move-Item -LiteralPath $resolved -Destination $backup
    }
    & $Python -m venv $environment
    if ($LASTEXITCODE -ne 0) { throw "Cannot create environment with $Python" }
}
$installOptions = @("install", "--disable-pip-version-check", "-e", $compiler)
if ($Offline) { $installOptions += @("--no-index", "--no-build-isolation") }
& $interpreter -m pip @installOptions
if ($LASTEXITCODE -ne 0) { throw "Compiler dependency installation failed" }
& $interpreter -m pip check --disable-pip-version-check
if ($LASTEXITCODE -ne 0) { throw "Compiler dependency check failed" }
& $interpreter (Join-Path $compiler "lmc.py") --version
if ($LASTEXITCODE -ne 0) { throw "Default compiler launcher failed" }
Write-Output "Compiler environment ready: $interpreter"

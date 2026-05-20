param(
    [string]$BuildDir = "build_current_mingw",
    [string]$QtPrefix = "C:/Qt/5.15.2/mingw81_64",
    [string]$Generator = "MinGW Makefiles",
    [int]$Jobs = 4,
    [switch]$SkipConfigure,
    [switch]$SkipBuild,
    [switch]$LaunchApp
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$BuildPath = Join-Path $Root $BuildDir
$ReportDir = Join-Path $Root "workflow_reports"
$Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$ReportPath = Join-Path $ReportDir "dev_health_$Stamp.txt"

New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null

function Write-Step {
    param([string]$Message)
    $line = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $Message
    Write-Host $line
    Add-Content -Path $ReportPath -Value $line
}

function Join-ProcessArguments {
    param([string[]]$Arguments)

    return ($Arguments | ForEach-Object {
        if ($_ -match '[\s"]') {
            '"' + ($_ -replace '"', '\"') + '"'
        }
        else {
            $_
        }
    }) -join " "
}

function Invoke-Logged {
    param(
        [string]$Name,
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$WorkingDirectory = $Root
    )

    Write-Step "START $Name"
    Add-Content -Path $ReportPath -Value ("> {0} {1}" -f $FilePath, ($Arguments -join " "))

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    $psi.Arguments = Join-ProcessArguments $Arguments
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    $exitCode = $process.ExitCode

    foreach ($text in @($stdout, $stderr)) {
        if (-not [string]::IsNullOrWhiteSpace($text)) {
            $text -split "`r?`n" | Where-Object { $_ -ne "" } | ForEach-Object {
                Write-Host $_
                Add-Content -Path $ReportPath -Value $_
            }
        }
    }

    if ($exitCode -ne 0) {
        Write-Step "FAIL $Name exit=$exitCode"
        throw "$Name failed with exit code $exitCode"
    }

    Write-Step "PASS $Name"
}

function Test-DslRuntime {
    $venvDir = Join-Path $Root "third_party\custom_dsp_language\compile\venv"
    $venvPython = Join-Path $venvDir "Scripts\python.exe"
    $venvCfg = Join-Path $venvDir "pyvenv.cfg"

    if (-not (Test-Path $venvPython)) {
        Write-Step "WARN DSL runtime check: venv python not found"
        return $false
    }

    if (-not (Test-Path $venvCfg)) {
        Write-Step "WARN DSL runtime check: pyvenv.cfg not found"
        return $false
    }

    $cfg = Get-Content $venvCfg -ErrorAction SilentlyContinue
    $executableLine = $cfg | Where-Object { $_ -like "executable = *" } | Select-Object -First 1
    if ($executableLine) {
        $configuredPython = $executableLine.Substring($executableLine.IndexOf("=") + 1).Trim()
        if (-not (Test-Path $configuredPython)) {
            Write-Step "WARN DSL runtime check: pyvenv.cfg points to missing Python: $configuredPython"
            return $false
        }
    }

    try {
        Invoke-Logged "dsl venv python probe" $venvPython @(
            "-c",
            "import sys; print(sys.version)"
        ) $venvDir
        Invoke-Logged "dsl antlr4 probe" $venvPython @(
            "-c",
            "import antlr4; print('antlr4 ok')"
        ) $venvDir
        Write-Step "PASS DSL runtime check"
        return $true
    }
    catch {
        Write-Step "WARN DSL runtime check: Python 3 / antlr4 runtime not available"
        return $false
    }
}

Set-Location $Root

Add-Content -Path $ReportPath -Value "ServoValvePlatform development health report"
Add-Content -Path $ReportPath -Value ("Root: {0}" -f $Root)
Add-Content -Path $ReportPath -Value ("BuildDir: {0}" -f $BuildPath)
Add-Content -Path $ReportPath -Value ("GeneratedAt: {0}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
Add-Content -Path $ReportPath -Value ""

$status = "PASS"

try {
    Write-Step "Checking required tools"
    Invoke-Logged "cmake version" "cmake" @("--version")
    Invoke-Logged "ctest version" "ctest" @("--version")
    $dslRuntimeReady = Test-DslRuntime

    if (-not $SkipConfigure) {
        New-Item -ItemType Directory -Force -Path $BuildPath | Out-Null
        Invoke-Logged "cmake configure" "cmake" @(
            "-S", $Root,
            "-B", $BuildPath,
            "-G", $Generator,
            "-DCMAKE_PREFIX_PATH=$QtPrefix"
        )
    }

    if (-not $SkipBuild) {
        Invoke-Logged "cmake build" "cmake" @(
            "--build", $BuildPath,
            "--config", "Debug",
            "-j", "$Jobs"
        )
    }

    Invoke-Logged "ctest" "ctest" @(
        "--test-dir", $BuildPath,
        "--output-on-failure"
    )

    $appExe = Join-Path $BuildPath "bin\ServoValvePlatform.exe"
    if (-not (Test-Path $appExe)) {
        throw "Application executable not found: $appExe"
    }
    Write-Step "Found application executable: $appExe"
    if (-not $dslRuntimeReady) {
        Write-Step "WARN DSL probe is skipped or degraded on this machine"
    }

    if ($LaunchApp) {
        $venvPy = Join-Path $Root "third_party\custom_dsp_language\compile\venv\Scripts\python.exe"
        if (-not (Test-Path $venvPy)) {
            throw "DSL Python venv not found: $venvPy"
        }
        $env:PYTHONHOME = ""
        $env:PYTHONPATH = ""
        $env:PYTHON = $venvPy
        $env:PATH = (Split-Path $venvPy -Parent) + ";" + $env:PATH
        Write-Step "Launching application"
        Start-Process -FilePath $appExe -WorkingDirectory $Root | Out-Null
    }
}
catch {
    $status = "FAIL"
    Write-Step ("ERROR " + $_.Exception.Message)
}
finally {
    Add-Content -Path $ReportPath -Value ""
    Add-Content -Path $ReportPath -Value ("RESULT: {0}" -f $status)
    Write-Host ""
    Write-Host ("RESULT: {0}" -f $status)
    Write-Host ("Report: {0}" -f $ReportPath)
}

if ($status -ne "PASS") {
    exit 1
}

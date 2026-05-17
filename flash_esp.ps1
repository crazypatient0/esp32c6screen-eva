$ErrorActionPreference = "Continue"

# Unset ALL MSYS/MINGW variables
$msysKeys = @('MSYSTEM','MINGW_PREFIX','MINGW_PACKAGE_PREFIX','MINGW_CHOST',
              'MSYSTEM_CARCH','MSYSTEM_CHOST','MSYSTEM_PREFIX',
              'MSYS_ENV_ID','MSYS_PS1','MSYS_TIP',
              'PYTHONUSERSITE')
foreach ($key in $msysKeys) {
    [System.Environment]::SetEnvironmentVariable($key, $null, 'Process')
}

$env:IDF_PATH = "D:\espidf"
$env:IDF_TOOLS_PATH = "D:\Espressif"
$env:IDF_PYTHON_ENV_PATH = "D:\Espressif\python_env\idf5.5_py3.11_env"
$env:PYTHONPATH = "D:\espidf\tools"

$env:PATH = (
    "D:\espidf\tools;" +
    "D:\Espressif\tools\cmake\3.30.2\bin;" +
    "D:\Espressif\tools\ninja\1.12.1;" +
    "D:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;" +
    "D:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin;" +
    "D:\Espressif\tools\idf-git\2.44.0\cmd;" +
    "D:\Espressif\python_env\idf5.5_py3.11_env\Scripts;" +
    "C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0"
)

Set-Location E:\esp32c6screen-eva

$python = "D:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe"
$idfPy = "D:\espidf\tools\idf.py"

Write-Host "=== MSYSTEM check: $($env:MSYSTEM) ==="
Write-Host "=== IDF_PATH: $env:IDF_PATH ==="

# Detect port
$ports = @("COM4", "COM5", "COM3", "COM6")
$port = $null
foreach ($p in $ports) {
    $test = Get-WmiObject Win32_SerialPort | Where-Object { $_.DeviceID -eq $p }
    if ($test) { $port = $p; break }
}
if (-not $port) {
    $allPorts = Get-WmiObject Win32_SerialPort
    if ($allPorts) { $port = $allPorts[0].DeviceID }
}
if (-not $port) { $port = "COM4" }
Write-Host "Using port: $port"

# Clean previous build artifacts but keep sdkconfig
Write-Host ""
Write-Host "=== Building ==="
& $python $idfPy build 2>&1 | Select-Object -Last 50
$rc = $LASTEXITCODE
if ($rc -ne 0) {
    Write-Host "BUILD FAILED: $rc"
    exit $rc
}

Write-Host ""
Write-Host "=== Flashing to $port ==="
& $python $idfPy -p $port flash 2>&1 | Select-Object -Last 50
$rc2 = $LASTEXITCODE
Write-Host "Flash exit: $rc2"
exit $rc2

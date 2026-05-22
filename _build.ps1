$ErrorActionPreference = "Continue"

$msysKeys = @('MSYSTEM','MINGW_PREFIX','MINGW_PACKAGE_PREFIX','MINGW_CHOST',
              'MSYSTEM_CARCH','MSYSTEM_CHOST','MSYSTEM_PREFIX',
              'MSYS_ENV_ID','MSYS_PS1','MSYS_TIP','PYTHONUSERSITE')
foreach ($key in $msysKeys) {
    [System.Environment]::SetEnvironmentVariable($key, $null, 'Process')
}

$env:IDF_PATH = "D:\espidf"
$env:IDF_TOOLS_PATH = "D:\Espressif"
$env:IDF_PYTHON_ENV_PATH = "D:\Espressif\python_env\idf5.5_py3.11_env"
$env:PATH = (
    "D:\espidf\tools;" +
    "D:\Espressif\tools\cmake\3.30.2\bin;" +
    "D:\Espressif\tools\ninja\1.12.1;" +
    "D:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;" +
    "D:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin;" +
    "D:\Espressif\python_env\idf5.5_py3.11_env\Scripts;" +
    "C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0"
)

Set-Location E:\esp32c6screen-eva

$python = "D:\Espressif\python_env\idf5.5_py3.11_env\Scripts\python.exe"
$idfPy = "D:\espidf\tools\idf.py"

& $python $idfPy build

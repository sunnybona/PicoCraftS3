@echo off
REM ===========================================================================
REM ESP-IDF v6.1.1 environment for Windows CMD.EXE
REM This machine uses a non-standard (eim-managed) tool layout, so the usual
REM export.bat cannot detect the tools. This script sets the variables and PATH
REM directly, mirroring the ESP-IDF PowerShell profile.
REM Usage:  call idf_env.bat
REM ===========================================================================

set "IDF_PATH=e:\esp\v6.1.1\esp-idf"
set "IDF_TOOLS_PATH=e:\esp\v6.1.1\tools"
set "IDF_PYTHON_ENV_PATH=e:\esp\v6.1.1\tools\python\v6.1.1\venv"
set "ESP_IDF_VERSION=6.1.1"
set "IDF_VERSION=6.1.1"
set "IDF_CCACHE_ENABLE=1"
set "CMAKE_BUILD_PARALLEL_LEVEL=2"
set "IDF_COMPONENT_LOCAL_STORAGE_URL=file://e:\esp\v6.1.1\tools"
set "ESP_ROM_ELF_DIR=e:\esp\v6.1.1\tools\esp-rom-elfs\20260528/"
set "ESP_CLANG_LIBS_PATH=e:\esp\v6.1.1\tools\esp-clang-libs\esp-21.1.3_20260408/esp-clang/lib"
set "OPENOCD_SCRIPTS=e:\esp\v6.1.1\tools\openocd-esp32\v0.12.0-esp32-20260831/openocd-esp32/share/openocd/scripts"

set "IDF_TOOL_PATHS=e:\esp\v6.1.1\tools\ccache\4.12.1\ccache-4.12.1-windows-x86_64;e:\esp\v6.1.1\tools\cmake\4.0.3\bin;e:\esp\v6.1.1\tools\dfu-util\0.11\dfu-util-0.11-win64;e:\esp\v6.1.1\tools\esp-clang\esp-21.1.3_20260408\esp-clang\bin;e:\esp\v6.1.1\tools\esp-clangd\esp-21.1.3_20260408\esp-clangd\bin;e:\esp\v6.1.1\tools\esp-rom-elfs\20260528\;e:\esp\v6.1.1\tools\esp32ulp-elf\2.38_20240113\esp32ulp-elf\bin;e:\esp\v6.1.1\tools\idf-exe\1.0.3\;e:\esp\v6.1.1\tools\ninja\1.12.1\;e:\esp\v6.1.1\tools\openocd-esp32\v0.12.0-esp32-20260831\openocd-esp32\bin;e:\esp\v6.1.1\tools\riscv32-esp-elf-gdb\17.1_20260402\riscv32-esp-elf-gdb\bin;e:\esp\v6.1.1\tools\riscv32-esp-elf\esp-15.2.0_20251204\riscv32-esp-elf\bin;e:\esp\v6.1.1\tools\xtensa-esp-elf-gdb\17.1_20260402\xtensa-esp-elf-gdb\bin;e:\esp\v6.1.1\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin;e:\esp\v6.1.1\tools\python\v6.1.1\venv\Scripts"

set "PATH=%IDF_TOOL_PATHS%;%PATH%"
set IDF_TOOL_PATHS=

echo ESP-IDF 6.1.1 CMD environment ready.
echo IDF_PATH=%IDF_PATH%

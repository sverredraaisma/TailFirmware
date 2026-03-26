@echo off
set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.4.1
set IDF_PYTHON=C:\Espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe
set IDF_PYTHON_ENV_PATH=C:\Espressif\python_env\idf5.4_py3.11_env
set MSYSTEM=
set ESP_ROM_ELF_DIR=C:\Espressif\tools\esp-rom-elfs\20241011
set PATH=C:\Espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\idf-git\2.44.0\cmd;C:\Espressif\python_env\idf5.4_py3.11_env\Scripts;%PATH%

cd /d %~dp0
%IDF_PYTHON% %IDF_PATH%\tools\idf.py %*

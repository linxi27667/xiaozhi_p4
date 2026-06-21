@echo off
REM Bypass Windows Store Python app execution aliases
REM Load ESP-IDF tools explicitly instead of relying on the caller's PATH
REM Skip component manager network check to avoid build hanging
if not defined IDF_PATH set "IDF_PATH=E:\MCU\esp32\.espressif\v5.5.3\esp-idf"
if not defined IDF_COMPONENT_CHECK_OR_UPDATE set IDF_COMPONENT_CHECK_OR_UPDATE=0
call "%IDF_PATH%\export.bat" >nul
python "%IDF_PATH%\tools\idf.py" %*

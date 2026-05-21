@echo off
REM Bypass Windows Store Python app execution aliases
REM Uses ESP-IDF Python explicitly instead of relying on PATH
"C:\Espressif\tools\python\v5.5.3\venv\Scripts\python.exe" -m idf.py %*

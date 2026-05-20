@echo off
cd /d %~dp0
call D:\Espressif\frameworks\esp-idf-v5.5.3\export.bat
idf.py build

@echo off


start "stsProxy" cmd /k "cd /d "C:\projects\contigoTriton\" && C:\projects\WPy64-310111\python-3.10.11.amd64\python.exe stsServoHandler.py" 
start "tritonGui" cmd /k "cd /d "C:\projects\contigoTriton\" && C:\projects\WPy64-310111\python-3.10.11.amd64\python.exe guiSts.py" 

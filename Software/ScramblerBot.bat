@echo off
start cmd /k "cd Python && python main.py"
timeout /t 2 /nobreak > NUL
start "" "GUI\index.html"
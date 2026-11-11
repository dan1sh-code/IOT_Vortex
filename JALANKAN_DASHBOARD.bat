@echo off
title VORTEX-X Mission Control Dashboard
echo ==============================================================
echo   VORTEX-X // GAMEPAD MISSION CONTROL LOCAL SERVER
echo ==============================================================
echo Membuka dashboard di browser...
start "" "http://localhost:8000/dashboard/index.html"
python -m http.server 8000 --directory "%~dp0."
pause


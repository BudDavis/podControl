@echo off
setlocal

:: Define the IP addresses of the remote machines
set "IP1=169.254.208.9"
set "IP2=169.254.172.31"

pskill \\copilot MoH_VR.exe

pskill \\crewchief MoH_VR.exe

taskkill /F /IM MoH_VR.exe

taskkill /F /IM cmd.exe

endlocal

echo Done.

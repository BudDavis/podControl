@echo off
setlocal

:: Define the IP addresses of the remote machines
set "IP1=169.254.208.9"
set "IP2=169.254.172.31"

:: Connect to the first remote machine and change to the specified directory, then run MoH_VR.exe
start cmd.exe /k "psexec -i \\crewchief "C:\Users\admin\Desktop\MoH\Windows\MoH_VR.exe""



:: Connect to the second remote machine and change to the specified directory, then run MoH_VR.exe
start cmd.exe /k "psexec -i \\copilot "C:\Users\test\Desktop\MoH\Windows\MoH_VR.exe""


start cmd.exe /k "C:\Users\admin\Desktop\Windows\MoH_VR.exe"


::psexec -s -i 0 \\169.254.208.9 "C:\Users\admin\Desktop\test.bat"
::psexec -s -i \\169.254.208.9 cmd /c "C:\Users\admin\Desktop\MoH\Windows\MoH_VR.exe"
::psexec -s -i 0 \\169.254.208.9 cmd /c "C:\Users\admin\Desktop\test.bat"



::working:: psexec -i \\crewchief "C:\Users\admin\Desktop\test.bat"
::working:: psexec -i \\crewchief "C:\Users\admin\Desktop\MoH\Windows\MoH_VR.exe"
::working:: psexec -i \\copilot "C:\Users\test\Desktop\MoH\Windows\MoH_VR.exe"

endlocal

echo Done.

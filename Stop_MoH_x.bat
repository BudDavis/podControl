 
setlocal

:: Define the IP addresses of the remote machines
set "IP1=169.254.208.9"
set "IP2=169.254.172.31"

pskill \\169.254.112.22 MoH_VR.exe

pskill \\169.254.112.11 MoH_VR.exe

taskkill /F /IM MoH_VR.exe

taskkill /F /IM cmd.exe

endlocal

echo Done.

---
description: How to safely compile and flash the midipops_promicro sketch using PlatformIO CLI
---
When multiple `.ino` sketches exist in the root directory, PlatformIO's builder can become confused and fail with compiling duplicate `setup()` references. 

To safely compile and flash the board from the terminal without breaking the VSCode workspace, we isolate the build process in a temporary folder.

// turbo
```powershell
mkdir pio_temp
cd pio_temp
& "C:\Users\zemuro\.platformio\penv\Scripts\pio.exe" project init --board sparkfun_promicro16
Copy-Item ..\midipops_promicro.ino src\main.ino
Copy-Item -Recurse ..\include include\
& "C:\Users\zemuro\.platformio\penv\Scripts\pio.exe" run -t upload
cd ..
Remove-Item -Recurse -Force pio_temp
```

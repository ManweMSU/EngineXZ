@echo off
ertbuild extension\xxcomex.ertproj -Nd
ertbuild installer\es_installer.ertproj -Nd
ertbuild ..\xx_xxsc\xxsc.ertproj -Nd
mkdir test
copy /Y extension\_build\windows_x64_debug\xxcomex.dll test\xxcomex.dll
copy /Y installer\_build\windows_x64_debug\es_installer.exe test\es_installer.exe
copy /Y ..\xx_xxsc\_build\windows_x64_debug\xxsc.exe test\xxsc.exe
test\es_installer.exe +
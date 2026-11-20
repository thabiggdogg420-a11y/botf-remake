@echo off
rem Builds glslangValidator at vulkan-sdk-1.4.335.0 (the glslang the fork's engine embeds) so the build-time WGSL table
rem hashes the same SPIR-V the engine produces at run time. Barclay, WebGPU trial r4.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set PATH=%APPDATA%\Python\Python312\Scripts;%PATH%
cd /d C:\Users\BiggDogg\Documents\ChatGPT\godot-webgpu\glslang
python -m cmake -S src-1.4.335 -B out -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_OPT=OFF -DGLSLANG_TESTS=OFF -DENABLE_HLSL=OFF -DCMAKE_MAKE_PROGRAM=ninja
if errorlevel 1 exit /b 1
python -m cmake --build out --target glslang-standalone -j 12

# Builds the patched web template (dlink, nothreads, release) at BelowNormal. Barclay, WebGPU trial r4.
$ErrorActionPreference = 'Continue'
$E = "C:\Users\BiggDogg\Documents\ChatGPT\godot-webgpu\emsdk"
$env:EMSDK = $E
$env:EMSDK_NODE = "$E\node\24.19.0_64bit\node.exe"
$env:EMSDK_PYTHON = "$E\python\3.13.3_64bit\python.exe"
$env:PATH = "$E;$E\upstream\emscripten;$E\node\24.19.0_64bit\bin;$E\node\24.19.0_64bit;" + $env:PATH
$env:PATH = "C:\Users\BiggDogg\Documents\ChatGPT\godot-webgpu\glslang\out\StandAlone;" + $env:PATH
$env:WGSL_CONVERT_CLI = "C:\Users\BiggDogg\Documents\ChatGPT\godot-webgpu\editor\wgsl_convert_cli.exe"
Set-Location "C:\Users\BiggDogg\Documents\ChatGPT\godot-webgpu\src"
$p = Start-Process -FilePath "C:\Program Files\Python312\python.exe" -ArgumentList "-m SCons platform=web target=template_release dlink_enabled=yes webgpu=yes opengl3=no threads=no -j14" -RedirectStandardOutput "..\build-template.log" -RedirectStandardError "..\build-template.err" -PassThru -NoNewWindow
try { $p.PriorityClass = 'BelowNormal' } catch {}
$p.WaitForExit()
"exit=" + $p.ExitCode

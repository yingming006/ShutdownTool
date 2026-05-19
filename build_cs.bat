@echo off
echo ========================================
echo   ShutdownTool - C# Build (Zero Setup)
echo ========================================
echo.
echo Compiling C# source...

set CSC=C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe

if not exist "%CSC%" (
    echo ERROR: csc.exe not found. .NET Framework 4.x is required.
    exit /b 1
)

"%CSC%" /target:winexe /win32icon:icon.ico /out:shutdown_tool.exe shutdown_tool.cs
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Compilation failed!
    exit /b 1
)

echo.
echo ========================================
echo   Build complete: shutdown_tool.exe
echo ========================================
dir shutdown_tool.exe 2>nul
echo.
echo NOTE: .NET Framework is pre-installed on Windows.
echo       No external dependencies needed.
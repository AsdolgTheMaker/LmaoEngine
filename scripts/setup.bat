@echo off
setlocal enabledelayedexpansion

echo ============================================
echo  LmaoEngine - Environment Setup
echo ============================================
echo.

set ERRORS=0

:: Check for CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [MISSING] CMake not found. Installing...
    winget install Kitware.CMake --accept-package-agreements --accept-source-agreements
    if !errorlevel! neq 0 (
        echo [ERROR] Failed to install CMake. Install manually from https://cmake.org/download/
        set /a ERRORS+=1
    ) else (
        echo [OK] CMake installed.
    )
) else (
    echo [OK] CMake found.
)

:: Check for Vulkan SDK (need glslc for shader compilation)
set GLSLC_FOUND=0
where glslc >nul 2>&1
if %errorlevel% equ 0 (
    set GLSLC_FOUND=1
)
if exist "C:\VulkanSDK" (
    for /d %%D in (C:\VulkanSDK\*) do (
        if exist "%%D\Bin\glslc.exe" set GLSLC_FOUND=1
    )
)
if defined VULKAN_SDK (
    if exist "%VULKAN_SDK%\Bin\glslc.exe" set GLSLC_FOUND=1
)

if %GLSLC_FOUND% equ 0 (
    echo [MISSING] Vulkan SDK not found. Installing...
    winget install KhronosGroup.VulkanSDK --accept-package-agreements --accept-source-agreements
    if !errorlevel! neq 0 (
        echo [ERROR] Failed to install Vulkan SDK.
        echo         Install manually from https://vulkan.lunarg.com/sdk/home
        set /a ERRORS+=1
    ) else (
        echo [OK] Vulkan SDK installed.
        echo      You may need to restart your terminal for VULKAN_SDK to be set.
    )
) else (
    echo [OK] Vulkan SDK found.
)

:: Check for Visual Studio build tools
where cl >nul 2>&1
if %errorlevel% neq 0 (
    :: Check if VS is installed but not in PATH
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
        echo [OK] Visual Studio 2022 found (run from Developer Command Prompt for cl.exe).
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" (
        echo [OK] VS Build Tools 2022 found (run from Developer Command Prompt for cl.exe).
    ) else (
        echo [MISSING] No C++ compiler found.
        echo           Install Visual Studio 2022 or Build Tools:
        echo           winget install Microsoft.VisualStudio.2022.BuildTools
        set /a ERRORS+=1
    )
) else (
    echo [OK] C++ compiler found.
)

:: Check for Git
where git >nul 2>&1
if %errorlevel% neq 0 (
    echo [MISSING] Git not found. Installing...
    winget install Git.Git --accept-package-agreements --accept-source-agreements
    if !errorlevel! neq 0 (
        echo [ERROR] Failed to install Git. Install manually from https://git-scm.com/
        set /a ERRORS+=1
    ) else (
        echo [OK] Git installed.
    )
) else (
    echo [OK] Git found.
)

echo.
echo ============================================
if %ERRORS% equ 0 (
    echo  All dependencies satisfied!
    echo  To build:
    echo    cmake -B build -DCMAKE_BUILD_TYPE=Release
    echo    cmake --build build --config Release
    echo.
    echo  To run:
    echo    build\Release\lmao_demo.exe
) else (
    echo  %ERRORS% dependency(ies) missing. Fix the errors above.
)
echo ============================================

endlocal

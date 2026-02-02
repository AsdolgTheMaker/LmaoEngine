#!/bin/bash
set -e

echo "============================================"
echo " LmaoEngine - Environment Setup"
echo "============================================"
echo

ERRORS=0

# Detect OS
OS="unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
fi

# Check CMake
if command -v cmake &>/dev/null; then
    echo "[OK] CMake found: $(cmake --version | head -1)"
else
    echo "[MISSING] CMake not found."
    if [[ "$OS" == "linux" ]]; then
        echo "  Install: sudo apt install cmake   (Debian/Ubuntu)"
        echo "       or: sudo dnf install cmake    (Fedora)"
    elif [[ "$OS" == "macos" ]]; then
        echo "  Install: brew install cmake"
    fi
    ERRORS=$((ERRORS + 1))
fi

# Check C++ compiler
if command -v g++ &>/dev/null || command -v clang++ &>/dev/null; then
    echo "[OK] C++ compiler found."
else
    echo "[MISSING] No C++ compiler found."
    if [[ "$OS" == "linux" ]]; then
        echo "  Install: sudo apt install build-essential"
    elif [[ "$OS" == "macos" ]]; then
        echo "  Install: xcode-select --install"
    fi
    ERRORS=$((ERRORS + 1))
fi

# Check Vulkan SDK / glslc
if command -v glslc &>/dev/null; then
    echo "[OK] glslc found (Vulkan shader compiler)."
elif [[ -n "$VULKAN_SDK" ]] && [[ -f "$VULKAN_SDK/bin/glslc" ]]; then
    echo "[OK] glslc found in VULKAN_SDK."
else
    echo "[MISSING] Vulkan SDK / glslc not found."
    if [[ "$OS" == "linux" ]]; then
        echo "  Install: sudo apt install vulkan-sdk"
        echo "       or: Download from https://vulkan.lunarg.com/sdk/home"
    elif [[ "$OS" == "macos" ]]; then
        echo "  Download from https://vulkan.lunarg.com/sdk/home"
    fi
    ERRORS=$((ERRORS + 1))
fi

# Check Vulkan runtime
if command -v vulkaninfo &>/dev/null; then
    echo "[OK] Vulkan runtime available."
else
    echo "[MISSING] Vulkan runtime not found."
    if [[ "$OS" == "linux" ]]; then
        echo "  Install: sudo apt install libvulkan1 vulkan-tools"
    fi
    ERRORS=$((ERRORS + 1))
fi

# Check Git
if command -v git &>/dev/null; then
    echo "[OK] Git found."
else
    echo "[MISSING] Git not found."
    ERRORS=$((ERRORS + 1))
fi

# Check for X11/Wayland dev headers (Linux GLFW requirement)
if [[ "$OS" == "linux" ]]; then
    if pkg-config --exists x11 2>/dev/null || pkg-config --exists wayland-client 2>/dev/null; then
        echo "[OK] Window system headers found."
    else
        echo "[MISSING] X11/Wayland development headers needed for GLFW."
        echo "  Install: sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev"
        echo "       or: sudo apt install libwayland-dev libxkbcommon-dev"
        ERRORS=$((ERRORS + 1))
    fi
fi

echo
echo "============================================"
if [[ $ERRORS -eq 0 ]]; then
    echo " All dependencies satisfied!"
    echo " To build:"
    echo "   cmake -B build -DCMAKE_BUILD_TYPE=Release"
    echo "   cmake --build build"
    echo
    echo " To run:"
    echo "   ./build/lmao_demo"
else
    echo " $ERRORS dependency(ies) missing. Fix the errors above."
fi
echo "============================================"

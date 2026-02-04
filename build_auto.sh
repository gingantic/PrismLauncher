#!/usr/bin/env bash
#
# AUTOMATIC BUILD SCRIPT - Uses CMake install system
# This is the CORRECT way - CMake handles everything automatically!
#
# README (Windows MSYS2/clang64):
# - Install MSYS2 and use the clang64 shell.
# - Install deps: pacman -S --needed base-devel mingw-w64-clang-x86_64-toolchain \
#   mingw-w64-clang-x86_64-cmake mingw-w64-clang-x86_64-ninja \
#   mingw-w64-clang-x86_64-qt6 mingw-w64-clang-x86_64-7zip
# - Run from repo root: ./build_auto.sh
#

set -e

echo "==== PrismLauncher Portable Builder (Automatic) ===="
echo "Using CMake install system - no manual copying needed!"

# ---- CONFIG ----
BUILD_DIR="build"
INSTALL_DIR="install"
PORTABLE_DIR="portable"

# 1. Clean old
echo "[1/5] Cleaning old build..."
rm -rf "$BUILD_DIR" "$INSTALL_DIR" "$PORTABLE_DIR"

# 2. Configure using official preset
echo "[2/5] Configuring..."
cmake --preset windows_mingw

# 3. Build Release
echo "[3/5] Building..."
cmake --build --preset windows_mingw --config Release

# 4. Install using CMake (THIS IS THE MAGIC!)
echo "[4/5] Installing using CMake..."
echo "  → CMake will automatically:"
echo "    • Copy all executables (prismlauncher.exe, updater, filelink)"
echo "    • Copy all DLLs and dependencies"
echo "    • Deploy Qt plugins correctly"
echo "    • Create qt.conf and qtlogging.ini"
echo "    • Handle everything!"

# Install to install/ directory first (matches official workflow)
# This ensures all components install correctly
cmake --install "$BUILD_DIR" --config Release --prefix "$INSTALL_DIR"
cmake --install "$BUILD_DIR" --config Release --prefix "$INSTALL_DIR" --component bundle
cmake --install "$BUILD_DIR" --config Release --prefix "$INSTALL_DIR" --component portable

# 5. Copy to final portable directory
echo "[5/5] Copying to portable directory..."
mkdir -p "$PORTABLE_DIR"
cp -r "$INSTALL_DIR"/* "$PORTABLE_DIR"/

# Generate manifest.txt (list of all files for updater)
echo "  → Generating manifest.txt..."
find "$PORTABLE_DIR" -type f | sed "s|^$PORTABLE_DIR/||" | sed "s|\\\\|/|g" | sort > "$PORTABLE_DIR/manifest.txt"

echo ""
echo "======================================="
echo "✅ Portable build created automatically!"
echo "Location: $PORTABLE_DIR/"
echo ""
echo "CMake install handled:"
echo "  ✓ All executables"
echo "  ✓ All DLLs and dependencies"
echo "  ✓ Qt plugins (correctly configured)"
echo "  ✓ qt.conf and qtlogging.ini"
echo "  ✓ portable.txt"
echo "  ✓ manifest.txt"
echo "======================================="

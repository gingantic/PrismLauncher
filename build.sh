#!/usr/bin/env bash
#
# README (Windows MSYS2/clang64):
# - Install MSYS2 and use the clang64 shell.
# - Install deps: pacman -S --needed base-devel mingw-w64-clang-x86_64-toolchain \
#   mingw-w64-clang-x86_64-cmake mingw-w64-clang-x86_64-ninja \
#   mingw-w64-clang-x86_64-qt6 mingw-w64-clang-x86_64-7zip
# - Run from repo root: ./build.sh
#

set -e

echo "==== PrismLauncher Portable Builder ===="

# ---- CONFIG ----
BUILD_DIR="build"
OUT_DIR="portable"
ZIP_NAME="PrismLauncher-Portable.zip"

# 1. Clean old
echo "[1/6] Cleaning old build..."
rm -rf "$BUILD_DIR" "$OUT_DIR" "$ZIP_NAME"

# 2. Configure using official preset
echo "[2/6] Configuring..."
cmake --preset windows_mingw

# 3. Build Release
echo "[3/6] Building..."
cmake --build --preset windows_mingw --config Release

# 4. Create portable structure
echo "[4/6] Preparing portable folder..."
mkdir -p "$OUT_DIR"

cp "$BUILD_DIR/Release/prismlauncher.exe" "$OUT_DIR/"
mkdir -p "$OUT_DIR/jars"
cp "$BUILD_DIR/jars/"*.jar "$OUT_DIR/jars/" 2>/dev/null || true

# Flag file for Prism portable mode
touch "$OUT_DIR/portable.txt"

# 5. Collect ALL MSYS2 runtime DLLs
echo "[5/6] Collecting MSYS2 DLL dependencies..."

ldd "$BUILD_DIR/Release/prismlauncher.exe" \
 | grep "/clang64/bin" \
 | awk '{print $3}' \
 | xargs -I{} cp -v {} "$OUT_DIR/" || true

# Common extras (some may not exist – ignore errors)
for f in \
  libwinpthread-1.dll \
  libgcc_s_seh-1.dll \
  libstdc++-6.dll \
  zlib1.dll \
  libarchive-*.dll \
  libbz2-*.dll \
  libzstd.dll ; do

  cp -v /clang64/bin/$f "$OUT_DIR/" 2>/dev/null || true
done

# 6. Deploy Qt
echo "[6/6] Running windeployqt..."
windeployqt "$OUT_DIR/prismlauncher.exe"

# Copy translations if built
cp -r "$BUILD_DIR/Release/translations" "$OUT_DIR/" 2>/dev/null || true

# # 7. Create final zip
# echo "Creating ZIP..."
# cd "$OUT_DIR"
# zip -r "../$ZIP_NAME" .
# cd ..

echo "======================================="
echo "Portable build created: $ZIP_NAME"
echo "Folder: $OUT_DIR/"
echo "Run with: $OUT_DIR/prismlauncher.exe"
echo "======================================="

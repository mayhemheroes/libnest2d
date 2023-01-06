#!/bin/bash
set -euo pipefail

# RLENV Build Script
# This script rebuilds the application from source located at /rlenv/source/libnest2d/
#
# Original image: ghcr.io/mayhemheroes/libnest2d:master
# Git revision: d834e37892e82c7c5b814b3740e1949e0a9c73c3

# ============================================================================
# Environment Variables
# ============================================================================
export CXX=clang++
export CC=clang

# ============================================================================
# REQUIRED: Change to Source Directory
# ============================================================================
cd /rlenv/source/libnest2d

# ============================================================================
# Clean Previous Build (recommended)
# ============================================================================
# Remove old build artifacts to ensure fresh rebuild
rm -f mayhem/fuzz_libnest 2>/dev/null || true
rm -f /fuzz_libnest 2>/dev/null || true

# Clean CMake-generated files (may be owned by root)
rm -f CMakeCache.txt 2>/dev/null || true
rm -rf CMakeFiles/ 2>/dev/null || true
rm -f cmake_install.cmake 2>/dev/null || true
rm -f Makefile 2>/dev/null || true
rm -f *.cmake 2>/dev/null || true
rm -rf src/CMakeFiles 2>/dev/null || true
rm -rf include/libnest2d/CMakeFiles 2>/dev/null || true
rm -rf tests/CMakeFiles 2>/dev/null || true

# ============================================================================
# Build Commands (NO NETWORK, NO PACKAGE INSTALLATION)
# ============================================================================
# Configure with CMake (in-source build to match original Dockerfile)
cmake . -DLIBNEST2D_HEADER_ONLY=OFF -DMAYHEM=1

# Build with make (using all cores for speed)
make -j$(nproc)

# ============================================================================
# Copy Artifacts (use 'cat >' for busybox compatibility)
# ============================================================================
# Copy fuzzer binary from build location to expected location
cat mayhem/fuzz_libnest > /fuzz_libnest

# ============================================================================
# Set Permissions
# ============================================================================
chmod 777 /fuzz_libnest 2>/dev/null || true

# 777 allows validation script (running as UID 1000) to overwrite during rebuild
# 2>/dev/null || true prevents errors if chmod not available

# ============================================================================
# REQUIRED: Verify Build Succeeded
# ============================================================================
if [ ! -f /fuzz_libnest ]; then
    echo "Error: Build artifact not found at /fuzz_libnest"
    exit 1
fi

# Verify executable bit
if [ ! -x /fuzz_libnest ]; then
    echo "Warning: Build artifact is not executable"
fi

# Verify file size (fuzzer should be at least a few KB)
SIZE=$(stat -c%s /fuzz_libnest 2>/dev/null || stat -f%z /fuzz_libnest 2>/dev/null || echo 0)
if [ "$SIZE" -lt 1000 ]; then
    echo "Warning: Build artifact is suspiciously small ($SIZE bytes)"
fi

echo "Build completed successfully: /fuzz_libnest (${SIZE} bytes)"

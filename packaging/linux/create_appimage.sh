#!/usr/bin/env bash
set -euo pipefail

# EdgeDrop Linux Standalone AppImage Packaging Script
# Packages binary, desktop entry, SVG icon, and Qt libraries via linuxdeploy

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
DIST_DIR="${PROJECT_ROOT}/dist"
APPDIR="${BUILD_DIR}/AppDir"

echo "=== EdgeDrop Linux AppImage Packaging ==="

mkdir -p "${DIST_DIR}"
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/scalable/apps"

# 1. Copy binary
cp "${BUILD_DIR}/edgedrop" "${APPDIR}/usr/bin/"
chmod +x "${APPDIR}/usr/bin/edgedrop"

# 2. Copy desktop file and icons
cp "${SCRIPT_DIR}/EdgeDrop.desktop" "${APPDIR}/usr/share/applications/"
cp "${SCRIPT_DIR}/edgedrop.svg" "${APPDIR}/usr/share/icons/hicolor/scalable/apps/"
cp "${SCRIPT_DIR}/edgedrop.svg" "${APPDIR}/edgedrop.svg"
cp "${SCRIPT_DIR}/EdgeDrop.desktop" "${APPDIR}/"

# 3. Download linuxdeploy if not present
LINUXDEPLOY_BIN="${BUILD_DIR}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT_PLUGIN="${BUILD_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"

if [ ! -f "${LINUXDEPLOY_BIN}" ]; then
    echo "Fetching linuxdeploy..."
    curl -sSL "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" -o "${LINUXDEPLOY_BIN}"
    chmod +x "${LINUXDEPLOY_BIN}"
fi

if [ ! -f "${LINUXDEPLOY_QT_PLUGIN}" ]; then
    echo "Fetching linuxdeploy Qt plugin..."
    curl -sSL "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" -o "${LINUXDEPLOY_QT_PLUGIN}"
    chmod +x "${LINUXDEPLOY_QT_PLUGIN}"
fi

# 4. Generate AppImage
export QML_SOURCES_PATHS="${PROJECT_ROOT}/desktop/qml"
OUTPUT_APPIMAGE="${DIST_DIR}/EdgeDrop-Linux-x86_64.AppImage"

"${LINUXDEPLOY_BIN}" \
    --appdir "${APPDIR}" \
    --desktop-file "${SCRIPT_DIR}/EdgeDrop.desktop" \
    --icon-file "${SCRIPT_DIR}/edgedrop.svg" \
    --plugin qt \
    --output appimage

mv EdgeDrop*.AppImage "${OUTPUT_APPIMAGE}"
chmod +x "${OUTPUT_APPIMAGE}"

echo "=== Linux AppImage Packaging Complete ==="
echo "AppImage: ${OUTPUT_APPIMAGE}"
sha256sum "${OUTPUT_APPIMAGE}"

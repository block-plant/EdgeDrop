#!/usr/bin/env bash
set -euo pipefail

# EdgeDrop macOS Standalone DMG Packaging Script
# Bundles Qt frameworks, strips debug symbols, applies ad-hoc codesign,
# injects the friendly-formal first-launch notice, and generates the DMG.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
DIST_DIR="${PROJECT_ROOT}/dist"
APP_BUNDLE="${BUILD_DIR}/edgedrop.app"
DEPLOY_TMP_DIR="/tmp/edgedrop_deploy_$$"
DEPLOY_APP="${DEPLOY_TMP_DIR}/EdgeDrop_Deploy.app"
STAGING_DIR="/tmp/edgedrop_staging_$$"

ARCH="$(uname -m)"
DMG_NAME="EdgeDrop-macOS-${ARCH}.dmg"
DMG_PATH="${DIST_DIR}/${DMG_NAME}"

echo "=== EdgeDrop macOS DMG Packaging ==="
echo "Project Root: ${PROJECT_ROOT}"
echo "Architecture: ${ARCH}"

if [ ! -d "${APP_BUNDLE}" ]; then
    echo "Error: Application bundle not found at ${APP_BUNDLE}. Run cmake build first." >&2
    exit 1
fi

mkdir -p "${DIST_DIR}"
rm -rf "${STAGING_DIR}" "${DEPLOY_TMP_DIR}"
mkdir -p "${STAGING_DIR}" "${DEPLOY_TMP_DIR}"

# 1. Clean stale frameworks/plugins and prepare clean app copy for deployment
echo "Preparing clean .app copy for deployment..."
rm -rf "${APP_BUNDLE}/Contents/Frameworks" "${APP_BUNDLE}/Contents/PlugIns"
cp -R "${APP_BUNDLE}" "${DEPLOY_APP}"

# 2. Deploy Qt dependencies into clean bundle copy if macdeployqt is available
MACDEPLOYQT_BIN=""
if [ -x "/opt/homebrew/opt/qt/bin/macdeployqt" ]; then
    MACDEPLOYQT_BIN="/opt/homebrew/opt/qt/bin/macdeployqt"
elif [ -x "/opt/homebrew/bin/macdeployqt" ]; then
    MACDEPLOYQT_BIN="/opt/homebrew/bin/macdeployqt"
elif command -v macdeployqt &>/dev/null; then
    MACDEPLOYQT_BIN="$(command -v macdeployqt)"
fi

if [ -n "${MACDEPLOYQT_BIN}" ]; then
    echo "Deploying Qt frameworks using ${MACDEPLOYQT_BIN}..."
    "${MACDEPLOYQT_BIN}" "${DEPLOY_APP}" \
        -qmldir="${PROJECT_ROOT}/desktop/qml" \
        -no-codesign || true

    # Clean any unused optional frameworks that macdeployqt over-scans
    rm -rf "${DEPLOY_APP}/Contents/Frameworks/Qt3D"* \
           "${DEPLOY_APP}/Contents/Frameworks/QtVirtualKeyboard"* \
           "${DEPLOY_APP}/Contents/Frameworks/QtPdf"* \
           "${DEPLOY_APP}/Contents/Frameworks/QtMultimedia"* \
           "${DEPLOY_APP}/Contents/Frameworks/QtSql"* \
           "${DEPLOY_APP}/Contents/Frameworks/QtQuickShapes"* \
           "${DEPLOY_APP}/Contents/Frameworks/QtQuickParticles"* 2>/dev/null || true
else
    echo "Notice: macdeployqt not found in PATH; using existing bundle layout."
fi

# 3. Strip Homebrew LC_RPATHs, recursively bundle transitive dylibs, and enforce Zero-Homebrew gate
echo "Bundling transitive dependencies and stripping Homebrew RPATHs..."
python3 "${SCRIPT_DIR}/bundle_dependencies.py" "${DEPLOY_APP}"

# 4. Strip local debugging symbols from binary
echo "Stripping local symbols from binary..."
strip -x "${DEPLOY_APP}/Contents/MacOS/edgedrop" || true

# 5. Count frameworks in deployed bundle
FRAMEWORK_COUNT=$(find "${DEPLOY_APP}/Contents/Frameworks" -mindepth 1 -maxdepth 1 2>/dev/null | wc -l | tr -d ' ')
echo "Bundled frameworks/libraries count: ${FRAMEWORK_COUNT}"

# 6. Clean extended attributes and apply ad-hoc code signature
echo "Applying ad-hoc code signature..."
xattr -cr "${DEPLOY_APP}"
codesign --force --deep --sign - "${DEPLOY_APP}"

# 7. Stage DMG contents
echo "Staging DMG contents..."
cp -R "${DEPLOY_APP}" "${STAGING_DIR}/EdgeDrop.app"
ln -s /Applications "${STAGING_DIR}/Applications"

# 8. Create friendly-formal unsigned build notice
cat > "${STAGING_DIR}/Read Me First.txt" << 'EOF'
A quick note before you open EdgeDrop

EdgeDrop is built by an independent creator and currently distributed as an unsigned, open-source binary. Because it isn’t signed with a paid Apple Developer or Microsoft certificate ($99–$499/yr), your operating system will show a standard first-launch warning:

• macOS: You may see “EdgeDrop cannot be opened because Apple cannot check it for malicious software” or “unidentified developer.”
  → Right-click (or Control-click) EdgeDrop.app, choose Open, and click Open again.
  → Or run in Terminal: xattr -cr /Applications/EdgeDrop.app

• Windows: SmartScreen may display “Windows protected your PC.”
  → Click “More info” and then select “Run anyway.”

• Linux: Ensure execution permission is set:
  → chmod +x EdgeDrop-*.AppImage

Every release is 100% open-source, reproducible, built on transparent GitHub Actions runners, and SHA-256 checksummed below. EdgeDrop makes zero network calls and runs entirely offline on your device.
EOF

# 9. Generate compressed DMG
echo "Generating ${DMG_PATH}..."
rm -f "${DMG_PATH}"
hdiutil create \
    -volname "EdgeDrop" \
    -srcfolder "${STAGING_DIR}" \
    -ov \
    -format UDZO \
    "${DMG_PATH}"

# 10. Clean up staging
rm -rf "${STAGING_DIR}" "${DEPLOY_TMP_DIR}"

# 11. Summary report
DMG_SIZE=$(du -h "${DMG_PATH}" | cut -f1 | tr -d ' ')
echo "=== DMG Packaging Complete ==="
echo "DMG File: ${DMG_PATH}"
echo "DMG File Size: ${DMG_SIZE}"
echo "Frameworks Count: ${FRAMEWORK_COUNT}"
shasum -a 256 "${DMG_PATH}"

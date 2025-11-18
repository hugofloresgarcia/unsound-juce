#!/bin/bash

# Build, notarize, and create release package for Unsound4All
# 
# Prerequisites:
# - Apple Developer account with signing certificate
# - Notarization credentials configured (APPLE_ID, APPLE_APP_SPECIFIC_PASSWORD, TEAM_ID)
# - Xcode Command Line Tools installed
# - CMake and build tools installed
#
# Usage:
#   ./scripts/build_and_release_unsound4all.sh [--skip-notarize] [--skip-dmg]

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
APP_NAME="Unsound4All Tape Looper"
BUNDLE_ID="com.unsound.unsound4all"
VERSION="1.0.0"

# Build configuration
BUILD_TYPE="Release"
ARCHITECTURE="arm64"  # Change to "x86_64" or "universal" as needed

# Signing configuration (set these environment variables or modify here)
SIGNING_IDENTITY="${APPLE_SIGNING_IDENTITY:-}"  # e.g., "Developer ID Application: Your Name (TEAM_ID)"
TEAM_ID="${APPLE_TEAM_ID:-}"
APPLE_ID="${APPLE_ID:-}"
APPLE_APP_SPECIFIC_PASSWORD="${APPLE_APP_SPECIFIC_PASSWORD:-}"

# Flags
SKIP_NOTARIZE=false
SKIP_DMG=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-notarize)
            SKIP_NOTARIZE=true
            shift
            ;;
        --skip-dmg)
            SKIP_DMG=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--skip-notarize] [--skip-dmg]"
            exit 1
            ;;
    esac
done

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    if [[ -z "$SIGNING_IDENTITY" && "$SKIP_NOTARIZE" == false ]]; then
        log_error "SIGNING_IDENTITY not set. Set APPLE_SIGNING_IDENTITY environment variable or modify script."
        exit 1
    fi
    
    if [[ "$SKIP_NOTARIZE" == false ]]; then
        if [[ -z "$TEAM_ID" ]]; then
            log_error "TEAM_ID not set. Set APPLE_TEAM_ID environment variable or modify script."
            exit 1
        fi
        
        if [[ -z "$APPLE_ID" ]]; then
            log_error "APPLE_ID not set. Set APPLE_ID environment variable or modify script."
            exit 1
        fi
        
        if [[ -z "$APPLE_APP_SPECIFIC_PASSWORD" ]]; then
            log_error "APPLE_APP_SPECIFIC_PASSWORD not set. Set APPLE_APP_SPECIFIC_PASSWORD environment variable or modify script."
            exit 1
        fi
    fi
    
    if ! command -v cmake &> /dev/null; then
        log_error "cmake not found. Please install CMake."
        exit 1
    fi
    
    if ! command -v codesign &> /dev/null; then
        log_error "codesign not found. Please install Xcode Command Line Tools."
        exit 1
    fi
    
    log_info "Prerequisites check passed."
}

# Build the application
build_app() {
    log_info "Building ${APP_NAME}..."
    
    cd "${PROJECT_ROOT}"
    
    # Create build directory if it doesn't exist
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    
    # Configure CMake
    log_info "Configuring CMake..."
    if [[ "$ARCHITECTURE" == "universal" ]]; then
        cmake .. -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
                 -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
    else
        cmake .. -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
                 -DCMAKE_OSX_ARCHITECTURES="${ARCHITECTURE}"
    fi
    
    # Build only the Unsound4AllApp target
    log_info "Building Unsound4AllApp..."
    cmake --build . --config "${BUILD_TYPE}" --target Unsound4AllApp -j$(sysctl -n hw.ncpu)
    
    log_info "Build completed successfully."
}

# Find the app bundle
find_app_bundle() {
    local app_bundle_path=""
    
    # Try common locations
    local possible_paths=(
        "${BUILD_DIR}/apps/unsound4all/${APP_NAME}.app"
        "${BUILD_DIR}/apps/unsound4all/Unsound4AllApp_artefacts/${BUILD_TYPE}/${APP_NAME}.app"
        "${BUILD_DIR}/apps/unsound4all/${BUILD_TYPE}/${APP_NAME}.app"
    )
    
    for path in "${possible_paths[@]}"; do
        if [[ -d "$path" ]]; then
            app_bundle_path="$path"
            break
        fi
    done
    
    if [[ -z "$app_bundle_path" ]]; then
        log_error "Could not find app bundle. Searched in:"
        for path in "${possible_paths[@]}"; do
            echo "  - $path"
        done
        exit 1
    fi
    
    echo "$app_bundle_path"
}

# Sign a single file or bundle
sign_file() {
    local file_path="$1"
    local entitlements="$2"
    
    log_info "Signing: $(basename "$file_path")"
    
    local sign_args=(
        --force
        --deep
        --timestamp
        --options runtime
        -s "$SIGNING_IDENTITY"
    )
    
    if [[ -n "$entitlements" && -f "$entitlements" ]]; then
        sign_args+=(--entitlements "$entitlements")
    fi
    
    sign_args+=("$file_path")
    
    codesign "${sign_args[@]}"
    
    # Verify signature
    if codesign --verify --verbose "$file_path" 2>&1 | grep -q "valid on disk"; then
        log_info "✓ Signature verified: $(basename "$file_path")"
    else
        log_error "Signature verification failed for: $file_path"
        codesign --verify --verbose "$file_path"
        exit 1
    fi
}

# Sign the app bundle and all its contents
sign_app_bundle() {
    local app_bundle="$1"
    
    log_info "Signing app bundle and all contents..."
    
    # Sign all dylibs and frameworks first
    log_info "Signing dylibs and frameworks..."
    find "$app_bundle" -type f \( -name "*.dylib" -o -name "*.framework" \) -print0 | while IFS= read -r -d '' file; do
        sign_file "$file"
    done
    
    # Sign all executables (except the main app executable, which will be signed last)
    log_info "Signing helper executables..."
    find "$app_bundle/Contents/MacOS" -type f -perm +111 -print0 | while IFS= read -r -d '' file; do
        if [[ "$file" != "$app_bundle/Contents/MacOS/${APP_NAME}" ]]; then
            sign_file "$file"
        fi
    done
    
    # Sign the main app bundle
    log_info "Signing main app bundle..."
    sign_file "$app_bundle"
    
    log_info "App bundle signing completed."
}

# Notarize the app
notarize_app() {
    local app_bundle="$1"
    
    if [[ "$SKIP_NOTARIZE" == true ]]; then
        log_warn "Skipping notarization (--skip-notarize flag set)."
        return
    fi
    
    log_info "Creating notarization zip..."
    local zip_path="${BUILD_DIR}/notarize.zip"
    rm -f "$zip_path"
    ditto -c -k --keepParent "$app_bundle" "$zip_path"
    
    log_info "Submitting app for notarization..."
    local notarize_output
    notarize_output=$(xcrun notarytool submit "$zip_path" \
        --apple-id "$APPLE_ID" \
        --password "$APPLE_APP_SPECIFIC_PASSWORD" \
        --team-id "$TEAM_ID" \
        --wait \
        2>&1)
    
    local submission_id
    submission_id=$(echo "$notarize_output" | grep -i "id:" | head -1 | awk '{print $NF}')
    
    if [[ -z "$submission_id" ]]; then
        log_error "Failed to get submission ID from notarization output:"
        echo "$notarize_output"
        exit 1
    fi
    
    log_info "Notarization submission ID: $submission_id"
    
    # Check notarization status
    log_info "Checking notarization status..."
    local status_output
    status_output=$(xcrun notarytool log "$submission_id" \
        --apple-id "$APPLE_ID" \
        --password "$APPLE_APP_SPECIFIC_PASSWORD" \
        --team-id "$TEAM_ID" \
        2>&1)
    
    if echo "$status_output" | grep -qi "status: accepted"; then
        log_info "✓ Notarization successful!"
        
        # Staple the notarization ticket
        log_info "Stapling notarization ticket..."
        xcrun stapler staple "$app_bundle"
        xcrun stapler validate "$app_bundle"
        
        log_info "✓ Notarization ticket stapled successfully."
    else
        log_error "Notarization failed or is still pending. Status output:"
        echo "$status_output"
        exit 1
    fi
    
    # Clean up
    rm -f "$zip_path"
}

# Create a DMG for distribution
create_dmg() {
    local app_bundle="$1"
    
    if [[ "$SKIP_DMG" == true ]]; then
        log_warn "Skipping DMG creation (--skip-dmg flag set)."
        return
    fi
    
    log_info "Creating DMG..."
    
    local dmg_name="${APP_NAME// /_}-${VERSION}-macOS-${ARCHITECTURE}.dmg"
    local dmg_path="${BUILD_DIR}/${dmg_name}"
    local temp_dmg_dir="${BUILD_DIR}/dmg_temp"
    
    # Clean up any existing temp directory
    rm -rf "$temp_dmg_dir"
    mkdir -p "$temp_dmg_dir"
    
    # Copy app to temp directory
    cp -R "$app_bundle" "$temp_dmg_dir/"
    
    # Create a symbolic link to Applications
    ln -s /Applications "$temp_dmg_dir/Applications"
    
    # Create DMG
    log_info "Building DMG image..."
    hdiutil create -volname "${APP_NAME}" \
        -srcfolder "$temp_dmg_dir" \
        -ov -format UDZO \
        -fs HFS+ \
        "$dmg_path"
    
    # Sign the DMG
    if [[ -n "$SIGNING_IDENTITY" ]]; then
        log_info "Signing DMG..."
        codesign --force --sign "$SIGNING_IDENTITY" --timestamp "$dmg_path"
    fi
    
    # Clean up
    rm -rf "$temp_dmg_dir"
    
    log_info "✓ DMG created: $dmg_path"
    
    # Create a zip as well (alternative distribution format)
    log_info "Creating ZIP archive..."
    local zip_name="${APP_NAME// /_}-${VERSION}-macOS-${ARCHITECTURE}.zip"
    local zip_path="${BUILD_DIR}/${zip_name}"
    rm -f "$zip_path"
    ditto -c -k --keepParent "$app_bundle" "$zip_path"
    
    if [[ -n "$SIGNING_IDENTITY" ]]; then
        log_info "Signing ZIP..."
        codesign --force --sign "$SIGNING_IDENTITY" --timestamp "$zip_path"
    fi
    
    log_info "✓ ZIP created: $zip_path"
}

# Main execution
main() {
    log_info "Starting build and release process for ${APP_NAME}"
    log_info "Version: ${VERSION}"
    log_info "Architecture: ${ARCHITECTURE}"
    log_info "Build type: ${BUILD_TYPE}"
    
    check_prerequisites
    build_app
    
    local app_bundle
    app_bundle=$(find_app_bundle)
    log_info "Found app bundle: $app_bundle"
    
    if [[ -n "$SIGNING_IDENTITY" ]]; then
        sign_app_bundle "$app_bundle"
    else
        log_warn "No signing identity provided. Skipping code signing."
    fi
    
    if [[ "$SKIP_NOTARIZE" == false && -n "$SIGNING_IDENTITY" ]]; then
        notarize_app "$app_bundle"
    fi
    
    create_dmg "$app_bundle"
    
    log_info "✓ Build and release process completed successfully!"
    log_info "App bundle: $app_bundle"
    if [[ "$SKIP_DMG" == false ]]; then
        log_info "DMG: ${BUILD_DIR}/${APP_NAME// /_}-${VERSION}-macOS-${ARCHITECTURE}.dmg"
        log_info "ZIP: ${BUILD_DIR}/${APP_NAME// /_}-${VERSION}-macOS-${ARCHITECTURE}.zip"
    fi
}

# Run main function
main "$@"


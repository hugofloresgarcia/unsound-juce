#!/bin/bash

# Build, notarize, and create release package for all apps
# 
# Prerequisites:
# - Apple Developer account with signing certificate
# - Notarization credentials configured (APPLE_ID, APPLE_APP_SPECIFIC_PASSWORD, TEAM_ID)
# - Xcode Command Line Tools installed
# - CMake and build tools installed
# - GitHub CLI (gh) installed and authenticated (for --github-release)
#
# Usage:
#   ./scripts/build_and_release_all_todo.sh [--skip-notarize] [--skip-dmg] [--skip-github] [--app APP_NAME]

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

# Define all apps: target_name:app_name:bundle_id:app_dir
declare -A APPS=(
    ["BasicApp"]="Basic Tape Looper:com.unsound.basic:basic"
    ["Text2SoundApp"]="Text2Sound Tape Looper:com.unsound.text2sound:text2sound"
    ["Text2Sound4AllApp"]="Text2Sound4All Tape Looper:com.unsound.text2sound4all:text2sound4all"
    ["EmbeddingSpaceSamplerApp"]="Embedding Space Sampler:com.unsound.embeddingsampler:embeddingsampler"
)

# Extract version from CMakeLists.txt (single source of truth)
# Looks for: project(TapeLooper VERSION X.Y.Z)
VERSION=$(grep -E "^project\(.*VERSION" "${PROJECT_ROOT}/CMakeLists.txt" | sed -E 's/.*VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/')
if [[ -z "$VERSION" ]]; then
    echo "ERROR: Could not extract version from CMakeLists.txt"
    echo "Make sure CMakeLists.txt contains: project(TapeLooper VERSION X.Y.Z)"
    exit 1
fi

# Build configuration
BUILD_TYPE="DEBUG"
ARCHITECTURE="arm64"  # Change to "x86_64" or "universal" as needed

# Signing configuration (set these environment variables or modify here)
SIGNING_IDENTITY="${APPLE_SIGNING_IDENTITY:-}"  # e.g., "Developer ID Application: Your Name (TEAM_ID)"
TEAM_ID="${APPLE_TEAM_ID:-}"
APPLE_ID="${APPLE_ID:-}"
APPLE_APP_SPECIFIC_PASSWORD="${APPLE_APP_SPECIFIC_PASSWORD:-}"

# Flags
SKIP_NOTARIZE=false
SKIP_DMG=false
SKIP_GITHUB=false
BUILD_SINGLE_APP=""

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
        --skip-github)
            SKIP_GITHUB=true
            shift
            ;;
        --app)
            BUILD_SINGLE_APP="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--skip-notarize] [--skip-dmg] [--skip-github] [--app APP_NAME]"
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

# Verify signing identity exists in keychain
verify_signing_identity() {
    if [[ -z "$SIGNING_IDENTITY" ]]; then
        return 0  # No signing identity set, skip verification
    fi
    
    log_info "Verifying signing identity in keychain..."
    
    # Check if the identity exists
    local identity_found
    identity_found=$(security find-identity -v -p codesigning 2>&1 | grep -F "$SIGNING_IDENTITY" || true)
    
    if [[ -z "$identity_found" ]]; then
        log_error "Signing identity not found in keychain: $SIGNING_IDENTITY"
        log_error ""
        log_error "Available signing identities:"
        security find-identity -v -p codesigning 2>&1 | grep -E "^[[:space:]]*[0-9]+\)" || true
        log_error ""
        log_error "To fix this:"
        log_error "1. Make sure your certificate is installed in Keychain Access"
        log_error "2. Check the exact name with: security find-identity -v -p codesigning"
        log_error "3. Set APPLE_SIGNING_IDENTITY to match the exact name (including quotes if needed)"
        log_error "4. If the keychain is locked, unlock it: security unlock-keychain"
        exit 1
    fi
    
    log_info "✓ Signing identity found: $SIGNING_IDENTITY"
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    if [[ -z "$SIGNING_IDENTITY" && "$SKIP_NOTARIZE" == false ]]; then
        log_error "SIGNING_IDENTITY not set. Set APPLE_SIGNING_IDENTITY environment variable or modify script."
        exit 1
    fi
    
    # Verify signing identity exists in keychain if provided
    if [[ -n "$SIGNING_IDENTITY" ]]; then
        verify_signing_identity
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
    local target_name="$1"
    log_info "Building ${target_name}..."
    
    cd "${PROJECT_ROOT}"
    
    # Create build directory if it doesn't exist
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    
    # Configure CMake (only if not already configured)
    if [[ ! -f "CMakeCache.txt" ]]; then
        log_info "Configuring CMake..."
        # Set minimum macOS deployment target to 11.0 (supports macOS 11+ including macOS 14)
        # This ensures the app can run on older macOS versions
        local cmake_args=(
            -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
            -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0"
        )
        
        if [[ "$ARCHITECTURE" == "universal" ]]; then
            cmake_args+=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64")
        else
            cmake_args+=(-DCMAKE_OSX_ARCHITECTURES="${ARCHITECTURE}")
        fi
        
        cmake .. "${cmake_args[@]}"
    fi
    
    # Build the target
    log_info "Building ${target_name}..."
    cmake --build . --config "${BUILD_TYPE}" --target "${target_name}" -j$(sysctl -n hw.ncpu)
    
    log_info "Build completed successfully for ${target_name}."
}

# Find the app bundle
find_app_bundle() {
    local app_name="$1"
    local app_dir="$2"
    local target_name="$3"
    local app_bundle_path=""
    
    # Try common locations
    local possible_paths=(
        "${BUILD_DIR}/apps/${app_dir}/${app_name}.app"
        "${BUILD_DIR}/apps/${app_dir}/${target_name}_artefacts/${BUILD_TYPE}/${app_name}.app"
        "${BUILD_DIR}/apps/${app_dir}/${BUILD_TYPE}/${app_name}.app"
    )
    
    for path in "${possible_paths[@]}"; do
        if [[ -d "$path" ]]; then
            app_bundle_path="$path"
            break
        fi
    done
    
    if [[ -z "$app_bundle_path" ]]; then
        log_error "Could not find app bundle for ${app_name}. Searched in:"
        for path in "${possible_paths[@]}"; do
            echo "  - $path"
        done
        return 1
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
    
    # Capture codesign output and error
    local codesign_output
    local codesign_exit_code
    
    if ! codesign_output=$(codesign "${sign_args[@]}" 2>&1); then
        codesign_exit_code=$?
        log_error "codesign failed with exit code $codesign_exit_code"
        echo "$codesign_output"
        
        # Check for specific keychain errors
        if echo "$codesign_output" | grep -qi "keychain"; then
            log_error ""
            log_error "Keychain error detected. Possible solutions:"
            log_error "1. Unlock your keychain: security unlock-keychain"
            log_error "2. Verify the signing identity exists: security find-identity -v -p codesigning"
            log_error "3. Make sure the certificate name matches exactly: $SIGNING_IDENTITY"
            log_error "4. Try unlocking the login keychain specifically:"
            log_error "   security unlock-keychain ~/Library/Keychains/login.keychain-db"
        fi
        
        exit 1
    fi
    
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
    local app_name="$2"
    
    log_info "Signing app bundle and all contents..."
    
    # Sign all dylibs and frameworks first
    log_info "Signing dylibs and frameworks..."
    find "$app_bundle" -type f \( -name "*.dylib" -o -name "*.framework" \) -print0 | while IFS= read -r -d '' file; do
        sign_file "$file"
    done
    
    # Sign all executables (except the main app executable, which will be signed last)
    log_info "Signing helper executables..."
    find "$app_bundle/Contents/MacOS" -type f -perm +111 -print0 | while IFS= read -r -d '' file; do
        if [[ "$file" != "$app_bundle/Contents/MacOS/${app_name}" ]]; then
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
    
    # Check for success indicators in JSON output
    # The output can be JSON with "status": "Accepted" or "statusCode": 0
    local is_accepted=false
    if echo "$status_output" | grep -qiE '"status"\s*:\s*"Accepted"'; then
        is_accepted=true
    elif echo "$status_output" | grep -qiE '"statusCode"\s*:\s*0'; then
        is_accepted=true
    elif echo "$status_output" | grep -qi "status: accepted"; then
        # Fallback for non-JSON output format
        is_accepted=true
    fi
    
    if [[ "$is_accepted" == true ]]; then
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
    local app_name="$2"
    
    if [[ "$SKIP_DMG" == true ]]; then
        log_warn "Skipping DMG creation (--skip-dmg flag set)."
        return
    fi
    
    log_info "Creating DMG..."
    
    local safe_app_name="${app_name// /_}"
    local dmg_name="${safe_app_name}-${VERSION}-macOS-${ARCHITECTURE}.dmg"
    local dmg_path="${BUILD_DIR}/${dmg_name}"
    local temp_dmg_dir="${BUILD_DIR}/dmg_temp_${safe_app_name}"
    
    # Clean up any existing temp directory
    rm -rf "$temp_dmg_dir"
    mkdir -p "$temp_dmg_dir"
    
    # Copy app to temp directory
    cp -R "$app_bundle" "$temp_dmg_dir/"
    
    # Create a symbolic link to Applications
    ln -s /Applications "$temp_dmg_dir/Applications"
    
    # Create DMG
    log_info "Building DMG image..."
    hdiutil create -volname "${app_name}" \
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
    local zip_name="${safe_app_name}-${VERSION}-macOS-${ARCHITECTURE}.zip"
    local zip_path="${BUILD_DIR}/${zip_name}"
    rm -f "$zip_path"
    ditto -c -k --keepParent "$app_bundle" "$zip_path"
    
    if [[ -n "$SIGNING_IDENTITY" ]]; then
        log_info "Signing ZIP..."
        codesign --force --sign "$SIGNING_IDENTITY" --timestamp "$zip_path"
    fi
    
    log_info "✓ ZIP created: $zip_path"
    echo "$dmg_path|$zip_path"
}

# Process a single app
process_app() {
    local target_name="$1"
    local app_info="$2"
    
    # Parse app info: app_name:bundle_id:app_dir
    IFS=':' read -r app_name bundle_id app_dir <<< "$app_info"
    
    log_info "=========================================="
    log_info "Processing app: ${app_name} (${target_name})"
    log_info "=========================================="
    
    # Build the app
    if ! build_app "$target_name"; then
        log_error "Failed to build ${target_name}"
        return 1
    fi
    
    # Find the app bundle
    local app_bundle
    if ! app_bundle=$(find_app_bundle "$app_name" "$app_dir" "$target_name"); then
        log_error "Failed to find app bundle for ${app_name}"
        return 1
    fi
    log_info "Found app bundle: $app_bundle"
    
    # Sign the app bundle
    if [[ -n "$SIGNING_IDENTITY" ]]; then
        sign_app_bundle "$app_bundle" "$app_name"
    else
        log_warn "No signing identity provided. Skipping code signing."
    fi
    
    # Notarize the app
    if [[ "$SKIP_NOTARIZE" == false && -n "$SIGNING_IDENTITY" ]]; then
        notarize_app "$app_bundle"
    fi
    
    # Create DMG/ZIP
    local assets
    assets=$(create_dmg "$app_bundle" "$app_name")
    
    log_info "✓ Completed processing ${app_name}"
    echo "$assets"
}

# Main execution
main() {
    log_info "Starting build and release process for all apps"
    log_info "Version: ${VERSION}"
    log_info "Architecture: ${ARCHITECTURE}"
    log_info "Build type: ${BUILD_TYPE}"
    
    check_prerequisites
    
    # Collect all assets for GitHub release
    local all_assets=()
    local failed_apps=()
    
    # Process each app
    for target_name in "${!APPS[@]}"; do
        # Skip if building single app and this isn't it
        if [[ -n "$BUILD_SINGLE_APP" && "$target_name" != "$BUILD_SINGLE_APP" ]]; then
            continue
        fi
        
        local app_info="${APPS[$target_name]}"
        local assets
        if assets=$(process_app "$target_name" "$app_info"); then
            # Add assets to list (format: dmg_path|zip_path)
            IFS='|' read -r dmg_path zip_path <<< "$assets"
            if [[ -f "$dmg_path" ]]; then
                all_assets+=("$dmg_path")
            fi
            if [[ -f "$zip_path" ]]; then
                all_assets+=("$zip_path")
            fi
        else
            failed_apps+=("$target_name")
            log_error "Failed to process ${target_name}"
        fi
    done
    
    # Report results
    log_info "=========================================="
    log_info "Build and release summary"
    log_info "=========================================="
    
    if [[ ${#failed_apps[@]} -gt 0 ]]; then
        log_error "Failed apps: ${failed_apps[*]}"
    else
        log_info "✓ All apps processed successfully!"
    fi
    
    log_info "Total assets created: ${#all_assets[@]}"
    for asset in "${all_assets[@]}"; do
        log_info "  - $asset"
    done
}

# Run main function
main "$@"


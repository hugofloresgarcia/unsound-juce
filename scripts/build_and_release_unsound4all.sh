#!/bin/bash

# Build, notarize, and create release package for Text2Sound4All
# 
# Prerequisites:
# - Apple Developer account with signing certificate
# - Notarization credentials configured (APPLE_ID, APPLE_APP_SPECIFIC_PASSWORD, TEAM_ID)
# - Xcode Command Line Tools installed
# - CMake and build tools installed
# - GitHub CLI (gh) installed and authenticated (for --github-release)
#
# Usage:
#   ./scripts/build_and_release_unsound4all.sh [--skip-notarize] [--skip-dmg] [--skip-github]

set -e  # Exit on error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
APP_NAME="Text2Sound4All Tape Looper"
BUNDLE_ID="com.unsound.text2sound4all"

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
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--skip-notarize] [--skip-dmg] [--skip-github]"
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
    log_info "Building ${APP_NAME}..."
    
    cd "${PROJECT_ROOT}"
    
    # Create build directory if it doesn't exist
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    
    # Configure CMake
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
    
    # Build only the Text2Sound4AllApp target
    log_info "Building Text2Sound4AllApp..."
    cmake --build . --config "${BUILD_TYPE}" --target Text2Sound4AllApp -j$(sysctl -n hw.ncpu)
    
    log_info "Build completed successfully."
}

# Find the app bundle
find_app_bundle() {
    local app_bundle_path=""
    
    # Try common locations
    local possible_paths=(
        "${BUILD_DIR}/apps/text2sound4all/${APP_NAME}.app"
        "${BUILD_DIR}/apps/text2sound4all/Text2Sound4AllApp_artefacts/${BUILD_TYPE}/${APP_NAME}.app"
        "${BUILD_DIR}/apps/text2sound4all/${BUILD_TYPE}/${APP_NAME}.app"
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

# Create GitHub release
create_github_release() {
    if [[ "$SKIP_GITHUB" == true ]]; then
        log_warn "Skipping GitHub release (--skip-github flag set)."
        return
    fi
    
    # Check if GitHub CLI is installed
    if ! command -v gh &> /dev/null; then
        log_warn "GitHub CLI (gh) not found. Skipping GitHub release."
        log_warn "Install it with: brew install gh"
        log_warn "Then authenticate with: gh auth login"
        return
    fi
    
    # Check if authenticated
    local auth_status_output
    if ! auth_status_output=$(gh auth status 2>&1); then
        log_warn "GitHub CLI not authenticated. Skipping GitHub release."
        log_warn "Authenticate with: gh auth login"
        return
    fi
    
    # Get repository name from git remote
    local repo_name
    local remote_url
    remote_url=$(git config --get remote.origin.url 2>&1)
    
    if [[ -z "$remote_url" ]]; then
        log_warn "No git remote 'origin' found. Skipping GitHub release."
        return
    fi
    
    # Extract repo name from various URL formats
    # git@github.com:user/repo.git -> user/repo
    # git@github-personal:user/repo.git -> user/repo
    # https://github.com/user/repo.git -> user/repo
    # https://github.com/user/repo -> user/repo
    
    # Use sed to extract user/repo from the URL
    # First try to match with .git suffix, then without
    repo_name=$(echo "$remote_url" | sed -E 's|.*[:/]([^/]+/[^/]+)\.git$|\1|')
    if [[ "$repo_name" == "$remote_url" ]]; then
        # No .git suffix, try without it
        repo_name=$(echo "$remote_url" | sed -E 's|.*[:/]([^/]+/[^/]+)$|\1|')
    fi
    
    # Validate that we got a user/repo format
    if [[ ! "$repo_name" =~ ^[^/]+/[^/]+$ ]]; then
        log_warn "Could not parse repository name from remote URL: $remote_url"
        log_warn "Got: $repo_name"
        log_warn "Skipping GitHub release."
        return
    fi
    
    log_info "Creating GitHub release for repository: $repo_name"
    
    # Create tag name
    local tag_name="v${VERSION}"
    
    # Check if tag already exists
    if git rev-parse "$tag_name" &> /dev/null 2>&1; then
        log_warn "Tag $tag_name already exists. Skipping tag creation."
    else
        # Create and push tag
        log_info "Creating git tag: $tag_name"
        git tag -a "$tag_name" -m "Release ${VERSION}" || {
            log_error "Failed to create git tag"
            return 1
        }
        
        log_info "Pushing tag to GitHub..."
        git push origin "$tag_name" || {
            log_error "Failed to push tag to GitHub"
            log_error "You may need to push manually: git push origin $tag_name"
            return 1
        }
        log_info "✓ Tag pushed successfully"
    fi
    
    # Prepare release notes
    local release_notes="Release ${VERSION} for macOS (${ARCHITECTURE})"
    release_notes+=$'\n\n'
    release_notes+="## Changes"
    release_notes+=$'\n'
    release_notes+="- Built and notarized for macOS ${ARCHITECTURE}"
    if [[ "$SKIP_NOTARIZE" == false ]]; then
        release_notes+=$'\n'
        release_notes+="- Notarized by Apple"
    fi
    
    # Collect assets to upload
    local assets=()
    local dmg_name="${APP_NAME// /_}-${VERSION}-macOS-${ARCHITECTURE}.dmg"
    local dmg_path="${BUILD_DIR}/${dmg_name}"
    local zip_name="${APP_NAME// /_}-${VERSION}-macOS-${ARCHITECTURE}.zip"
    local zip_path="${BUILD_DIR}/${zip_name}"
    
    if [[ -f "$dmg_path" ]]; then
        assets+=("$dmg_path")
    fi
    
    if [[ -f "$zip_path" ]]; then
        assets+=("$zip_path")
    fi
    
    if [[ ${#assets[@]} -eq 0 ]]; then
        log_warn "No release assets found (DMG/ZIP). Creating release without assets."
    fi
    
    # Create release
    log_info "Creating GitHub release..."
    local release_args=(
        "release" "create" "$tag_name"
        "--title" "Release ${VERSION}"
        "--notes" "$release_notes"
    )
    
    # Add assets
    for asset in "${assets[@]}"; do
        release_args+=("$asset")
    done
    
    local gh_output
    if gh_output=$(gh "${release_args[@]}" 2>&1); then
        log_info "✓ GitHub release created successfully!"
        log_info "Release URL: https://github.com/${repo_name}/releases/tag/${tag_name}"
    else
        log_error "Failed to create GitHub release"
        echo "$gh_output"
        log_error "You may need to create it manually at: https://github.com/${repo_name}/releases/new"
        return 1
    fi
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
    
    # Create GitHub release (non-critical, so continue even if it fails)
    if ! create_github_release; then
        log_warn "GitHub release failed, but build completed successfully."
    fi
    
    log_info "✓ Build and release process completed successfully!"
    log_info "App bundle: $app_bundle"
    if [[ "$SKIP_DMG" == false ]]; then
        log_info "DMG: ${BUILD_DIR}/${APP_NAME// /_}-${VERSION}-macOS-${ARCHITECTURE}.dmg"
        log_info "ZIP: ${BUILD_DIR}/${APP_NAME// /_}-${VERSION}-macOS-${ARCHITECTURE}.zip"
    fi
}

# Run main function
main "$@"


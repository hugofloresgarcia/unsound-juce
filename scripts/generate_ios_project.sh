#!/bin/bash

# Script to generate Xcode project for iOS (iPad) build of LayerCake
# Usage: ./scripts/generate_ios_project.sh [TeamID]
# If TeamID is provided, it will attempt to set it in the project generation, 
# otherwise it will leave it blank for manual setting in Xcode.
#
# Note: This only builds LayerCake for iPad. Tests and other apps are excluded.

set -e

# Move to project root if script is run from scripts/ dir
if [[ "$(basename "$PWD")" == "scripts" ]]; then
    cd ..
fi

# Check for Xcode and set DEVELOPER_DIR if needed
if [[ -z "$DEVELOPER_DIR" ]] && [[ -d "/Applications/Xcode.app/Contents/Developer" ]]; then
    echo "Setting DEVELOPER_DIR to /Applications/Xcode.app/Contents/Developer"
    export DEVELOPER_DIR="/Applications/Xcode.app/Contents/Developer"
fi

BUILD_DIR="build/ios"
# Use argument if provided, otherwise fall back to APPLE_TEAM_ID env var
TEAM_ID="${1:-$APPLE_TEAM_ID}"

echo "Generating iOS project for LayerCake in $BUILD_DIR..."

mkdir -p "$BUILD_DIR"

CMAKE_ARGS=(
    -G Xcode
    -DCMAKE_SYSTEM_NAME=iOS
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0
    -DCMAKE_XCODE_ATTRIBUTE_IDECustomDerivedDataLocation="$PWD/build/ios/DerivedData"
    -DJUCE_BUILD_EXAMPLES=OFF
    -DJUCE_BUILD_EXTRAS=OFF
    -DLINK_BUILD_TESTS=OFF
)

if [[ -n "$TEAM_ID" ]]; then
    echo "Using Development Team ID: $TEAM_ID"
    CMAKE_ARGS+=("-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=$TEAM_ID")
fi

cmake -S . -B "$BUILD_DIR" "${CMAKE_ARGS[@]}"

echo ""
echo "Done! LayerCake iOS project generated at $BUILD_DIR/TapeLooper.xcodeproj"
echo "Open with: open $BUILD_DIR/TapeLooper.xcodeproj"
echo ""
echo "In Xcode:"
echo "  1. Select the LayerCakeApp target"
echo "  2. Set your Development Team in Signing & Capabilities"
echo "  3. Build and run on your iPad"


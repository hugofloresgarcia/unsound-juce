#!/bin/bash

# Define paths
BUILD_DIR="$(cd "$(dirname "$0")/../build" && pwd)"
VST3_SOURCE="$BUILD_DIR/apps/layercake/LayerCakePlugin_artefacts/Debug/VST3/LayerCakePlugin.vst3"
AU_SOURCE="$BUILD_DIR/apps/layercake/LayerCakePlugin_artefacts/Debug/AU/LayerCakePlugin.component"

# Destination paths (User Library)
VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3/LayerCakePlugin.vst3"
AU_DEST="$HOME/Library/Audio/Plug-Ins/Components/LayerCakePlugin.component"

echo "======================================================="
echo "LayerCake Logic Pro X Launcher"
echo "======================================================="

# Check build artifacts
if [ ! -d "$VST3_SOURCE" ]; then
    echo "Warning: VST3 source not found at $VST3_SOURCE"
fi

if [ ! -d "$AU_SOURCE" ]; then
    echo "Error: AU source not found at $AU_SOURCE"
    echo "Logic Pro X requires the AU component."
    echo "Did you build the project?"
    exit 1
fi

# Kill Logic if running
echo "Closing Logic Pro if running..."
killall "Logic Pro" 2>/dev/null || true

# Clean old copies
echo "Cleaning old plugins..."
rm -rf "$VST3_DEST"
rm -rf "$AU_DEST"

# Install new copies
echo "Installing plugins to $HOME/Library/Audio/Plug-Ins/..."
mkdir -p "$(dirname "$VST3_DEST")"
mkdir -p "$(dirname "$AU_DEST")"

if [ -d "$VST3_SOURCE" ]; then
    cp -r "$VST3_SOURCE" "$VST3_DEST"
    echo "Installed VST3."
fi

if [ -d "$AU_SOURCE" ]; then
    cp -r "$AU_SOURCE" "$AU_DEST"
    echo "Installed AU."
fi

# Force Audio Component Rescan (helps Logic see the update)
# echo "Killing AudioComponentRegistrar to force rescan..."
# killall AudioComponentRegistrar 2>/dev/null

echo "======================================================="
echo "Launching Logic Pro X..."
echo "======================================================="

open -a "Logic Pro"

echo ""
echo "Streaming logs from ~/Library/Logs/LayerCake/LayerCake.log..."
echo "Press Ctrl+C to stop streaming logs (Logic will continue running)."
echo "======================================================="

# Create log dir if it doesn't exist (it will be created by plugin, but valid for tail)
mkdir -p "$HOME/Library/Logs/LayerCake"
touch "$HOME/Library/Logs/LayerCake/LayerCake.log"

tail -f "$HOME/Library/Logs/LayerCake/LayerCake.log"

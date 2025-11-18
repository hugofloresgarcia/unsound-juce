# Building and Releasing Unsound4All for macOS

This guide explains how to build, sign, notarize, and create a release package for the Unsound4All application.

## Prerequisites

### 1. Apple Developer Account
- You need an active Apple Developer account ($99/year)
- Enroll at: https://developer.apple.com/programs/

### 2. Code Signing Certificate
You need a "Developer ID Application" certificate for distribution outside the App Store:

1. Log in to [Apple Developer Portal](https://developer.apple.com/account/)
2. Go to **Certificates, Identifiers & Profiles**
3. Create a new certificate:
   - Type: **Developer ID Application**
   - Follow the instructions to create and download the certificate
4. Install the certificate in Keychain Access

### 3. App-Specific Password for Notarization
1. Go to [Apple ID Account Page](https://appleid.apple.com/)
2. Sign in and go to **Security** section
3. Under **App-Specific Passwords**, click **Generate Password**
4. Give it a name (e.g., "Notarization")
5. Copy the generated password (you'll need this)

### 4. Team ID
Find your Team ID:
1. Go to [Apple Developer Portal](https://developer.apple.com/account/)
2. Your Team ID is shown in the top-right corner (e.g., `ABC123DEF4`)

### 5. Signing Identity Name
Find your signing identity:
```bash
security find-identity -v -p codesigning
```
Look for "Developer ID Application: Your Name (TEAM_ID)" and copy the full name.

## Setup

Set the following environment variables before running the script:

```bash
export APPLE_SIGNING_IDENTITY="Developer ID Application: Your Name (TEAM_ID)"
export APPLE_TEAM_ID="ABC123DEF4"
export APPLE_ID="your.apple.id@example.com"
export APPLE_APP_SPECIFIC_PASSWORD="abcd-efgh-ijkl-mnop"
```

Or create a `.env` file in the project root (make sure it's in `.gitignore`):

```bash
# .env
APPLE_SIGNING_IDENTITY="Developer ID Application: Your Name (TEAM_ID)"
APPLE_TEAM_ID="ABC123DEF4"
APPLE_ID="your.apple.id@example.com"
APPLE_APP_SPECIFIC_PASSWORD="abcd-efgh-ijkl-mnop"
```

Then source it before running:
```bash
source .env
```

## Usage

### Basic Build and Release

```bash
./scripts/build_and_release_unsound4all.sh
```

This will:
1. Build the app in Release mode
2. Sign the app bundle and all its contents (dylibs, frameworks, executables)
3. Submit for notarization and wait for completion
4. Staple the notarization ticket
5. Create a DMG and ZIP for distribution

### Skip Notarization

If you want to build and sign but skip notarization (faster for testing):

```bash
./scripts/build_and_release_unsound4all.sh --skip-notarize
```

### Skip DMG Creation

If you only want the signed app bundle:

```bash
./scripts/build_and_release_unsound4all.sh --skip-dmg
```

### Build for Different Architectures

Edit the script to change the `ARCHITECTURE` variable:
- `arm64` - Apple Silicon only
- `x86_64` - Intel only  
- `universal` - Universal binary (both architectures)

Or modify the script to accept architecture as a parameter.

## Output Files

After successful completion, you'll find:

- **App Bundle**: `build/apps/unsound4all/Unsound4AllApp_artefacts/Release/Unsound4All Tape Looper.app`
- **DMG**: `build/Unsound4All_Tape_Looper-1.0.0-macOS-arm64.dmg`
- **ZIP**: `build/Unsound4All_Tape_Looper-1.0.0-macOS-arm64.zip`

## Troubleshooting

### "No signing identity found"

Make sure your certificate is installed in Keychain Access and matches the `APPLE_SIGNING_IDENTITY` value exactly. Check with:
```bash
security find-identity -v -p codesigning
```

### "Notarization failed"

- Check that your Apple ID has access to notarization (requires paid developer account)
- Verify the app-specific password is correct
- Check the notarization logs:
  ```bash
  xcrun notarytool log <submission-id> --apple-id "$APPLE_ID" --password "$APPLE_APP_SPECIFIC_PASSWORD" --team-id "$TEAM_ID"
  ```

### "App bundle not found"

The script searches common build output locations. If your build outputs to a different location, modify the `find_app_bundle()` function in the script.

### "Dylib signing failed"

Make sure all dylibs (including ONNX Runtime) are copied to the app bundle before signing. The CMakeLists.txt should handle this, but verify:
- `libonnxruntime.dylib` is in `Contents/MacOS/`
- All model files are in `Contents/Resources/`

## Manual Steps (Alternative)

If you prefer to do things manually:

### 1. Build
```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build . --config Release --target Unsound4AllApp
```

### 2. Sign Dylibs
```bash
codesign --force --sign "Developer ID Application: Your Name (TEAM_ID)" \
  --timestamp --options runtime \
  "Unsound4All Tape Looper.app/Contents/MacOS/libonnxruntime.dylib"
```

### 3. Sign App Bundle
```bash
codesign --force --deep --sign "Developer ID Application: Your Name (TEAM_ID)" \
  --timestamp --options runtime \
  "Unsound4All Tape Looper.app"
```

### 4. Verify Signature
```bash
codesign --verify --verbose "Unsound4All Tape Looper.app"
spctl --assess --verbose "Unsound4All Tape Looper.app"
```

### 5. Notarize
```bash
# Create zip
ditto -c -k --keepParent "Unsound4All Tape Looper.app" notarize.zip

# Submit
xcrun notarytool submit notarize.zip \
  --apple-id "$APPLE_ID" \
  --password "$APPLE_APP_SPECIFIC_PASSWORD" \
  --team-id "$TEAM_ID" \
  --wait

# Staple
xcrun stapler staple "Unsound4All Tape Looper.app"
```

### 6. Create DMG
```bash
hdiutil create -volname "Unsound4All Tape Looper" \
  -srcfolder "Unsound4All Tape Looper.app" \
  -ov -format UDZO \
  -fs HFS+ \
  Unsound4All_Tape_Looper-1.0.0.dmg
```

## Distribution

Once you have a signed, notarized app:

1. **Test on a clean Mac** (without development tools) to ensure it runs
2. **Upload DMG or ZIP** to your distribution platform
3. **Users can download and run** - macOS Gatekeeper will recognize the notarization

## Additional Resources

- [Apple Code Signing Guide](https://developer.apple.com/library/archive/documentation/Security/Conceptual/CodeSigningGuide/)
- [Notarization Documentation](https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution)
- [JUCE Code Signing](https://juce.com/learn/code-signing)


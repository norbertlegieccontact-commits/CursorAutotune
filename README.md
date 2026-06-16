# Norby Autotune

JUCE 8 pitch-correction plugin scaffold for the VoxTune / NØRBY autotune project.

## Current implementation

- JUCE plugin target with AU on macOS and VST3/Standalone formats via CMake.
- APVTS parameters matching the project contract.
- YIN pitch detector with autocorrelation fallback.
- Key/scale quantizer for automatic correction.
- MIDI target-note support.
- Time-domain pitch shifter for a first working real-time correction path.
- Graphical mode node state with copy-on-write snapshots and XML serialization.
- WebView editor shell that dispatches pitch telemetry to JavaScript at 30 fps.

The current shifter is intentionally simple so the plugin is usable early. The architecture leaves clear module boundaries for replacing it with the target phase-vocoder/PSOLA implementation and for adding LPC formant processing.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

JUCE 8.0.12 is fetched by CMake. Platform plugin bundles are emitted by JUCE under the build tree.

## Build and install for Logic Pro on macOS

These commands are intended for the macOS Terminal app.

1. Install Apple's command line tools if you have not done this before:

```sh
xcode-select --install
```

2. Install CMake if it is not already installed:

```sh
brew install cmake
```

If `brew` is not available, install Homebrew first from <https://brew.sh>.

3. Clone the repository and switch to the plugin branch:

```sh
git clone https://github.com/norbertlegieccontact-commits/CursorAutotune.git
cd CursorAutotune
git checkout cursor/add-autotune-project-rules-e22f
```

4. Build the Audio Unit plugin for Logic:

```sh
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-mac --config Release
```

5. Install the plugin into Logic's Audio Units folder:

```sh
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
cp -R "build-mac/NorbyAutotune_artefacts/Release/AU/Norby Autotune.component" "$HOME/Library/Audio/Plug-Ins/Components/"
```

6. Restart the Audio Unit scanner:

```sh
killall -9 AudioComponentRegistrar 2>/dev/null || true
rm -f "$HOME/Library/Caches/AudioUnitCache/com.apple.audiounits.cache"
```

7. Open Logic Pro and add the plugin on an audio track:

```text
Audio FX -> VoxTune -> Norby Autotune
```

If Logic does not show the plugin, open Logic's Plugin Manager and rescan Audio Units.

On Ubuntu/Linux builders, install the JUCE GUI/WebView dependencies first:

```sh
sudo apt-get update
sudo apt-get install -y g++ libasound2-dev libfreetype-dev libfontconfig1-dev \
  libgl1-mesa-dev libcurl4-openssl-dev libx11-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libgtk-3-dev libwebkit2gtk-4.1-dev
```


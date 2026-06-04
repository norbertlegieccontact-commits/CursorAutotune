# Norby Autotune

JUCE 8 pitch-correction plugin scaffold for the VoxTune / NØRBY autotune project.

## Current implementation

- JUCE plugin target with VST3 and Standalone formats via CMake.
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

On Ubuntu/Linux builders, install the JUCE GUI/WebView dependencies first:

```sh
sudo apt-get update
sudo apt-get install -y g++ libasound2-dev libfreetype-dev libfontconfig1-dev \
  libgl1-mesa-dev libcurl4-openssl-dev libx11-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libgtk-3-dev libwebkit2gtk-4.1-dev
```


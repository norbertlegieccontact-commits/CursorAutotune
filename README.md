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


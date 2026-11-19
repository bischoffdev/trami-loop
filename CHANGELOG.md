# Changelog

All notable changes to `trami-loop` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-11-19

### Added
- **External VST3 / AudioUnit Plugin Hosting**: Host third-party VST3 (macOS & Windows) and AudioUnit (macOS) effect plugins on Clips, Tracks, and the Master Bus with directory scanning, bypass, reordering, native plugin editor windows, and Base64 state persistence (zero built-in effects).
- **Master Bus Track & Automation**: Docked Master Bus lane at the bottom of the timeline with Master Mute All (`M`), Master Solo All (`S`), Master Volume slider + editable dB readout, Master `fx` rack, `Env` toggle, live stereo peak meter with latched `CLIP` indicator (click to reset), and drawable Master Volume Automation curve.
- **Per-Track Live Peak Meters & FX**: 30 Hz smooth-decay stereo peak meters and per-track `fx` rack buttons in every track header.
- **Per-Clip FX & Bottom Bar**: Clip bottom bar with clip name, clickable `fx` badge, and clickable pitch/transpose selector.
- **Groove Quantize / Groove Pool Selector**: Built-in 16th-note swing, 8th-note shuffle, MPC, and triplet feel presets in the song toolbar.
- **Workflow Toolbar Toggles**: Auto-Crossfade (`X-Fade`), Envelope Lock (`Env Lock`), and Ripple Edit (`Ripple`).
- **Multitrack Song Timeline**: Select (`V`), Paint (`P`), Erase (`E`), Split (`S`), Join (`J`), Reverse (`R`), timeline Markers (`M`), multi-point clip/track envelopes, paint-to-set loop region in the ruler, resizable splitters, and window-relative zoom.
- **Audio Engine & I/O**: Tempo-synced Rubber Band time-stretching (`LOOP`, `1-SHOT`, `DISK` modes), live hot-swap editing during playback (reversing, transposing, gain/envelope changes heard immediately), sample browser with audition preview, in-place recording with count-in & latency compensation, self-contained `.trami` project bundles, undo/redo, and offline WAV mixdown export.
- **Packaging & Versioning**: Single-source `VERSION` file embedded in app UI, macOS `Info.plist`, Windows `.exe` metadata, and GitHub Release archives; 100% standalone Windows `.exe` (`/MT` static CRT) and macOS Universal Binary (`arm64;x86_64`) with macOS microphone and VST3/AU entitlements.

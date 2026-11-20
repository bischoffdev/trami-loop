# Changelog

All notable changes to `trami-loop` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] - 2026-11-20

### Added
- **32-Color Track Picker**: Click any track's left color bar to choose from 32 curated colors (or reset to Auto); colors apply to the track header, all clips on that track, paint previews, and the FX Rack, and persist in `.trami` projects.
- **Multi-Track Selection & Linked Controls**: Select multiple tracks (`Cmd/Ctrl+click` to toggle, `Shift+click` for range) to Mute, Solo, Record-Arm, change track color, and adjust Volume or Pan **relatively** across all selected tracks.
- **Zoomable Scrollbars**: Custom horizontal and vertical scrollbars with draggable left/right and top/bottom end-caps for 1:1 smooth zooming.
- **FX Rack Workflow Improvements**:
  - Single modeless floating FX Rack window (updates in place when switching between Clip, Track, and Master FX) with matching color coding and target banner.
  - **`Bypass All`** button inside the FX Rack plus quick right-click / `Alt+click` bypass on Track, Clip, and Master `fx` badges and context menus.
  - **`Jump to Clip`**, **`Jump to Track`**, and **`Track N FX`** navigation buttons in the FX Rack header.
  - Clear Clip/Track identification (`Clip FX: <name> (Track N, Bar X.Y)`).
- **Track Rename Propagation**: Renaming a track immediately updates clip names across that track.

### Fixed
- **Windows Stack Overflow**: Moved `Sequencer` audio-thread scratch buffers to flat heap-allocated vectors (`sizeof(ScratchBuffers)` ~96 bytes) so Windows' 1 MB default stack no longer overflows.
- **Windows & macOS VST3/AU Stability**:
  - Added Windows SEH crash protection (`__try`/`__except`), OS error dialog suppression, dead-man's-pedal blacklisting, and non-blocking incremental plugin scanning.
  - Fixed audio-thread `PluginHost` map/chain data race during `syncFromPattern` and added 16-channel stereo/sidechain-safe `processBlock` bus handling.
- **Loop-Off Playback**: Clips starting inside a blue loop region now play their full length when loop playback is turned off.
- **Ruler Horizontal Zoom**: Restored vertical drag below the blue loop bar in the timeline ruler to zoom horizontally.
- **UI Encoding**: Replaced raw UTF-8 em-dash literals that caused mojibake (`â`) in window titles and tooltips.

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

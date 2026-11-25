# Changelog

All notable changes to **TramiLoop** (`trami-loop`) will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.5.0] - Unreleased

### Added
- **Plugin Delay Compensation (PDC) Per Channel & Per Project**:
  - Automatic sample-accurate delay compensation across channels with differing VST3/AU plugin latencies using 65,536-sample per-channel circular delay lines in `Sequencer`.
  - Queries live `AudioPluginInstance::getLatencySamples()` on Clips, Tracks, and the Master Bus and syncs reported latencies into the song model and `.trami` project file (`pdcEnabled`, `projectLatencySamples`, per-track `latencySamples`, and per-slot `latencySamples`).
  - Offline WAV mixdown export (`WavExporter`) automatically prerolls and strips total project PDC latency so exported audio starts on sample 0.
  - Added `PDC` toggle and per-slot, per-channel, and project latency readouts in the FX Rack, Mixer, and Track Properties panels.
- **Per-Track Recording Input & Mono/Stereo Selection with Live Input Metering**:
  - Added per-track hardware input channel (`inputChannel`) and Mono/Stereo mode (`inputStereo`) selection in Track Headers (`In 1 M` / `In 1-2 S` button and context menu), Mixer channel strips, and Track Properties → General, persisted in `.trami` project files.
  - Real-time per-channel input peak measurement in `AudioRecorder` (even while stopped/idle) reflects live incoming levels on armed tracks in the track Volume Slider capsule LEDs, the track header bottom peak meter (unified Mono bar vs. dual Stereo bars), and the Mixer channel strip vertical meter.
- **Transport Bar Performance Indicator**:
  - Studio One–style `Performance` LCD box in the transport bar displaying dual stacked horizontal load meters (real-time Audio CPU/DSP callback load and Voice/Peak load) with a detailed live tooltip (Audio CPU %, Voice/Peak Load %, Active Voices, and PDC Latency).
- **Alternating Second-Measure Background Shading**:
  - Shaded every second measure (Measures 2, 4, 6, …) with a lighter background column overlay and brighter bar boundary lines across the Track View grid, Bar Ruler, Master Bus automation lane, and Arranger grid.
- **Toolbar Button Vector Icons**:
  - Added custom vector icons to all Track View toolbar tool and mode buttons (`Select`, `Paint`, `Erase`, `Split`, `Join`, `Marker`, `X-Fade`, `Env Lock`, and `Ripple`) matching the top application toolbar styling.
- **Project Name in Application Title Bar**:
  - Displayed the currently open project name in both the native OS window title bar (`TramiLoop v0.5.0 - <Project Name>`) and the top application bar, updating automatically on New, Open, Save, Undo/Redo, and song rename.

### Fixed
- **Reversed Clip Waveform & Multi-Point Curve Reversal**:
  - Fixed reversed clip waveform rendering in `PatternGrid` for both Loop and One-Shot/Disk clips (`offsetSteps` and wrap-around sample indexing).
  - Reversing a clip with **Envelope Lock** (`Env Lock`) enabled now automatically mirrors the clip's multi-point volume and pan envelopes, swaps `fadeInSteps` and `fadeOutSteps` (inverting `Fast` $\leftrightarrow$ `Slow` curve shapes), and reverses track automation points within the clip's span.
  - Fixed `Song::splitClip` audio offsets when slicing a reversed clip.

## [0.4.0] - 2026-11-22

### Added
- **macOS `.dmg` & Windows Inno Setup Installers**: Added automated GitHub Release installer packaging (`TramiLoop-v<version>-macOS.dmg` with `/Applications` drag-and-drop symlink and `TramiLoop-v<version>-Windows-Setup.exe` per-user installer with Start Menu shortcut and `.trami` file association) alongside portable `.zip` archives.
- **Custom Application Icon**: Added custom tilted neon loop application icon across macOS `.icns`, Windows `.ico`, runtime window/dock icon, and top-left toolbar badge.
- **Code Signing Support**: Added optional macOS Developer ID + Apple Notarization and Windows Authenticode / SignPath Foundation code signing support in CI.
- **README Screenshot & Installation Guide**: Added a 2x Retina workspace screenshot (`assets/screenshot.png`) and first-launch Gatekeeper/SmartScreen instructions to `README.md`.

### Changed
- **TramiLoop Branding**: Renamed the application display name, bundle (`TramiLoop.app` / `TramiLoop.exe`), window title, toolbar label, and installer packages to **TramiLoop** (while keeping the GitHub repository URL as `trami-loop`).

### Fixed
- **Master Volume Envelope Zoom Scaling**: Synced `MasterBusLane::stepPx_` with `PatternGrid::getStepWidth()` on initial construction and every zoom change so the Master volume automation curve scales in lockstep with timeline zoom.
- **Viewport Stability on Undo/Redo**: Replaced `scrollToKeepPlayheadVisible()` during Undo/Redo when playback is stopped with direct horizontal and vertical scroll offset preservation so undoing/redoing envelope points never shifts the track view.

## [0.3.0] - 2026-11-22

### Added
- **Multi-Purpose Bottom Dock (`Explorer` | `Properties` | `Mixer` | `FX`)**: Consolidated all detached windows into a single resizable tabbed dock at the bottom of the workspace with a live context readout and collapse toggle.
- **ACID-Style Dual-Pane Explorer**: Left folder-only directory tree with draggable splitter and right multi-column audio file & subfolder browser (double-click or press Enter to open subfolders, drag audio files directly onto tracks, compact audition/waveform bar at the bottom).
- **Docked Track Mixer**: Side-by-side horizontally scrollable track channel strips with track color bars, `FX` buttons (left-click to inspect, right-click/`Alt+click` to bypass), `M` and `S` buttons, pan sliders, vertical volume faders, live 30 Hz stereo peak meters, editable dB readouts, bidirectional selection sync with track headers, and a pinned **Master** channel strip.
- **Signal-Flow FX Rack View**: Replaced the detached FX dialog and plugin dropdown with a docked **searchable VST3/AU Plugin Browser** (live filter + `+` / double-click insert) paired with a horizontal hardware-style **FX Rack** (`IN → [Module] → OUT`) supporting **`Track FX`**, **`Clip FX`**, and **`Master FX`** scopes with one-click cross-switching and timeline jump actions.
- **Floating Non-Modal Plugin Editors**: External VST3/AU plugin UI windows now stay floating above the main window (`alwaysOnTop`) while remaining completely modeless.
- **Selectable Time Signatures**: Click the Time Signature button in the transport bar to select `2/4`, `3/4`, `4/4`, `5/4`, `6/4`, `7/4`, `3/8`, `6/8`, `7/8`, `9/8`, `12/8`, `2/2`, or **Custom...** (1–16 beats per bar, beat unit 2/4/8/16); updates ruler/grid/Master bar lines, metronome bar-1 accents, transport bar/beat readouts, empty-project bar length, and `.trami` project persistence.
- **Stretch View Zoom & Pan**: Added `-` / `1.0x` / `+` / `1:1` zoom controls (`1.0x`–`16.0x`), mouse-wheel and pinch-to-zoom anchored at the cursor, horizontal panning, and a draggable overview scrollbar in the docked Properties → Stretch view.
- **Expanded Undo/Redo Coverage**: Point-by-point Undo/Redo for Master Volume automation, track/clip multi-point envelopes, track envelope toggles, and Stretch transient marker edits.

### Changed
- **Toolbar Cleanup**: Removed the redundant Groove preset dropdown in favor of the Transport Bar's Swing control.
- **Inactive Loop Indicator**: Greyed out the ruler loop bar and vertical loop region shading when loop playback is turned off.

### Fixed
- **Mid-Clip Loop Wrap-Around**: Chased active mid-clip voices at the exact loop boundary offset on loop wrap so tracks that start before the loop region continue playing on the 2nd and subsequent loop passes.
- **Master Clip Indicator**: Synced `CLIP` LED state cleanly between `Sequencer`, `MasterBusLane`, and the Mixer's Master strip, wired click-to-reset to clear the latched state in both views, and changed clip detection to `> 1.0001f` to avoid false-positive clipping on normalized `0.0 dBFS` loops.
- **Explorer Subfolder Crash**: Fixed a use-after-free crash when opening a subfolder from the Explorer's right-hand multi-column list.
- **Stretch Marker Clear**: Removed automatic transient re-detection inside `loadFromModel()` so clicking **Clear Markers** leaves zero markers instead of repopulating 24 detected transients.

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

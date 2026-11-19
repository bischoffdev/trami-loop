# trami-loop

A fast, straightforward multitrack loop-based audio DAW for macOS and Windows.

Built around a single horizontal **Song** timeline with tempo-synced time-stretching, external VST3/AudioUnit plugin hosting, and instant live editing, `trami-loop` provides an immediate, distraction-free environment for building tracks and full songs.

---

## Key Features

- **Multitrack Song Timeline**:
  - Dynamic tracks with individual volume, pan (with clickable dB/Pan readouts), mute, solo, record arm, live stereo peak meters, and track type (`LOOP`, `1-SHOT`, `DISK`).
  - Selectable grid snap resolution: **1 Bar, 1/2, 1/4, 1/8, 1/16, and 1/32** (32-step/bar base resolution).
  - Full clip editing tools: **Select (`V`)**, **Paint (`P`)**, **Erase (`E`)**, **Split (`S`)**, **Join (`J`)**, **Reverse (`R`)**, and timeline **Markers (`M`)**.
  - Clip bottom bar displaying clip name, clickable **`fx`** badge, and clickable pitch/transpose selector.
  - Paint directly in the timeline ruler to define or adjust the loop region.
  - Live real-time editing: reversing, transposing, moving, or adjusting envelopes on playing clips is reflected immediately in audio playback.

- **External VST3 / AudioUnit Plugin Hosting (Zero Built-In Effects)**:
  - Host external **VST3** (macOS & Windows) and **AudioUnit** (macOS) effect plugins on individual **Clips**, **Tracks**, or the **Master Bus**.
  - Scan system plugin directories, reorder/bypass slots, open native plugin UI windows, and persist plugin states with the project.

- **Master Bus Track & Automation**:
  - Docked at the bottom of the timeline with Master **Mute All (`M`)** and **Solo All (`S`)**, Master Volume slider + dB readout, **`fx`** rack, **`Env`** toggle, live stereo peak meter, and a latched **`CLIP`** indicator (click to reset).
  - Synchronized **Master Volume Automation** lane (left-click to add/drag points, right-click a point to delete).

- **Multi-Point Envelopes & Track Automation**:
  - Drawable multi-point Volume and Pan envelopes on clips, plus track Volume / Pan / FX automation lanes.
  - Left-click on a curve to add and drag points; right-click a point to delete.

- **Groove Pool & Workflow Toggles**:
  - **Groove Quantize / Groove Pool** selector in the song toolbar (Straight, 16th Note Swing 55%/62%/68%, 8th Note Shuffle 60%/67%, MPC 16th 58%/64%, Triplet Feel).
  - **`X-Fade`** (Auto-Crossfade overlapping clips on the same track), **`Env Lock`** (move track automation points together with clips), and **`Ripple`** (deleting clips shifts downstream clips earlier).

- **Tempo-Synced Time-Stretching (Rubber Band)**:
  - Loops automatically time-stretch to the song's BPM and conform to project key/transpose settings.
  - One-shots play at their natural pitch and speed; Disk-Based tracks stream long audio files.
  - Background rendering via `StretchCache` keeps the real-time audio thread and UI non-blocking.

- **Built-in Sample Browser, Drag-and-Drop & Recording**:
  - Direct import for WAV and MP3 files with automatic resampling to the project sample rate.
  - Built-in directory browser with instant audition/preview and drag-and-drop onto tracks or empty space.
  - In-place audio recording with 1-bar metronome count-in and automatic input latency compensation.

- **Project Bundles, Undo/Redo & Offline WAV Export**:
  - Self-contained project folders (`project.trami` JSON + `samples/` audio folder) with multi-level Undo (`Cmd+Z`) / Redo (`Cmd+Shift+Z`).
  - Offline WAV mixdown export (16-bit / 24-bit at 44.1 kHz or 48 kHz).

---

## Opening Prebuilt Releases (macOS & Windows)

Prebuilt binaries from GitHub Releases are currently **unsigned** (no Apple Developer ID or Windows Authenticode certificate is configured yet), so macOS Gatekeeper and Windows SmartScreen will show a security warning on first launch.

### macOS

After unzipping `trami-loop-macOS.zip`, remove the download quarantine attribute in Terminal before launching:

```bash
xattr -cr /path/to/trami-loop.app
```

*(Alternatively, right-click `trami-loop.app` → **Open**, or allow it under **System Settings → Privacy & Security → Open Anyway**.)*

### Windows

After unzipping `trami-loop-Windows.zip` and double-clicking `trami-loop.exe`:

1. Click **More info** in the *"Windows protected your PC"* SmartScreen dialog.
2. Click **Run anyway**.

*(You can also right-click `trami-loop.exe` → **Properties** → check **Unblock** → **Apply**.)*

---

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Space` | Play / Pause |
| `0` | Stop Playback |
| `Return` | Return to Start (Rewind) |
| `V` | Select / Move Tool |
| `P` | Paint / Draw Tool |
| `E` | Erase Tool |
| `S` | Split Tool |
| `J` | Join Selected Clips |
| `R` | Reverse Selected Clip(s) |
| `M` | Add Timeline Marker at Playhead |
| `F` | Toggle Playhead Follow (Auto-Scroll) |
| `B` | Toggle Sample Browser |
| `Cmd + Z` / `Ctrl + Z` | Undo |
| `Cmd + Shift + Z` / `Ctrl + Y` | Redo |
| `Cmd + N` / `Ctrl + N` | New Project |
| `Cmd + O` / `Ctrl + O` | Open Project Folder |
| `Cmd + S` / `Ctrl + S` | Save Project Folder |
| `Cmd + E` / `Ctrl + E` | Export Mixdown to WAV |
| `Cmd + +` / `Cmd + =` | Zoom In (Horizontal) |
| `Cmd + -` | Zoom Out (Horizontal) |
| `Cmd + 0` | Reset Zoom |
| `Cmd + Shift + +` / `Cmd + Shift + -` | Increase / Decrease Track Height |

---

## Building from Source

### Prerequisites
- CMake 3.24 or higher
- C++20 compliant compiler (Apple Clang 14+, Clang 14+, GCC 12+, MSVC 2022+)

All third-party dependencies (JUCE 8, Rubber Band, Catch2) are automatically fetched via CMake at configure time.

### Build Instructions (macOS & Windows)

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build application and unit tests
cmake --build build --config Release --parallel

# Run test suite (94 unit tests)
ctest --test-dir build -C Release --output-on-failure
```

### Launching

On macOS:
```bash
open build/src/TramiLoop_artefacts/Release/trami-loop.app
```

On Windows:
```cmd
.\build\src\TramiLoop_artefacts\Release\trami-loop.exe
```

---

## Architecture & Code Structure

The codebase is organized into clean, modular layers:
- `src/model/`: Pure C++ domain model (`Song`, `Pattern`, `Sample`, `Timing`, `UndoManager`). Zero GUI dependencies, completely unit-testable.
- `src/engine/`: Real-time audio engine running lock-free on the audio callback (`AudioEngine`, `Sequencer`, `SnapshotExchange`, `SnapshotBuilder`, `PluginHost`, `TimeStretcher`, `StretchCache`, `AudioRecorder`).
- `src/io/`: File and project I/O (`ProjectSerializer`, `SampleImporter`, `WavExporter`, legacy project/metadata readers).
- `src/ui/`: JUCE Component hierarchy (`MainContainer`, `PatternView`, `PatternGrid`, `TrackHeader`, `PluginRackDialog`, `SampleBrowserPanel`, `TransportBar`, `TramiLookAndFeel`).
- `tests/`: Catch2 unit test suite (94 test cases) covering musical timing & grooves, model consistency, real-time sequencer & plugin hooks, time stretching, serialization, and WAV export.

---

## License

This project is licensed under the GNU Affero General Public License v3 (AGPLv3).
The repository is maintained as a private repository.

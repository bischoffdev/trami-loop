# trami-loop

A fast, straightforward pattern-based beat and arrangement DAW inspired by **Acid Music** and **Creator (Atari ST)**.

Combining tempo-synced time-stretching for audio loops and natural one-shots with Creator's horizontal pattern-to-arranger workflow, `trami-loop` provides an immediate, distraction-free environment for making beats and song structures.

---

## Key Features

- **Pattern View (Dynamic Tracks & 1-to-1 Track Patterns)**:
  - Each track is its own pattern (e.g., Track 1 = Kick, Track 2 = Snare), edited together in horizontal lanes and sequenced across the Arranger.
  - Starts with 1 track; add more tracks dynamically with "+ Add Track" or by dropping files.
  - Individual track volume, pan, mute, and solo controls.
  - Selectable raster size: **1 Bar, 1/2, 1/4, 1/8, 1/16, and 1/32** with high-resolution 32-step/bar grid snapping.
  - Adjustable swing slider (0% to 100%) with instant audio preview and `S` shortcut toggle.
  - Color-coded clips with sample identifiers and loop/one-shot badge indicators (`[L]` / `[1]`).
  - Interactive grid editing: click to place, drag to move across tracks and steps, right-click to delete.
  - Synchronized playhead tracking audio playback.

- **Arranger View (Multi-Track Song Sequencer)**:
  - Sequence and layer patterns across arranger tracks to structure complete songs.
  - Mute, solo, volume, and pan per arranger track.
  - Global swing adjustment (0% to 100%) matching the groove of the patterns.
  - Pattern management: add, duplicate, rename, and delete patterns.
  - Double-click any pattern block to jump directly into the Pattern Editor.

- **Tempo-Synced Time-Stretching (Rubber Band)**:
  - Loops automatically time-stretch to the song's BPM based on their bar length.
  - One-shots play at their natural pitch and speed without stretching.
  - Background rendering via `StretchCache` ensures the real-time audio thread and UI never block.

- **Built-in Sample Browser & Drag-and-Drop**:
  - Direct import for WAV and MP3 files with automatic resampling to the project sample rate.
  - Built-in directory browser with instant audition/preview button.
  - Drag-and-drop audio files directly from the browser or OS Finder/Explorer onto existing tracks to assign them, or drop onto empty space / "+ Add Track" to automatically create new tracks.

- **In-Place Audio Recording**:
  - Record directly into the Pattern View (creates a new sample and track pattern) or the Arranger View (creates a pattern take).
  - 1-bar metronome count-in.
  - Automatic audio device input latency compensation.

- **Project Bundles & Undo/Redo**:
  - Self-contained project folders (`project.trami` JSON + `samples/` audio folder).
  - Snapshot-based multi-level Undo (`Cmd+Z`) and Redo (`Cmd+Shift+Z`).

- **Offline WAV Mixdown Export**:
  - High-quality offline mixdown rendering of the complete arrangement.
  - Supported formats: 16-bit / 44.1 kHz, 24-bit / 44.1 kHz, 16-bit / 48 kHz, and 24-bit / 48 kHz.

---

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Space` | Play / Stop |
| `Return` | Return to Start (Rewind) |
| `Cmd + Z` / `Ctrl + Z` | Undo |
| `Cmd + Shift + Z` / `Ctrl + Y` | Redo |
| `Cmd + S` / `Ctrl + S` | Save Project Folder |
| `Cmd + O` / `Ctrl + O` | Open Project Folder |
| `M` | Metronome Click Toggle |
| `S` | Swing Toggle (Straight / 50% Swing) |
| `Up` / `Down` | Tempo +/- 5 BPM |
| `Cmd + +` / `Cmd + =` | Zoom In (Time / Horizontal) |
| `Cmd + -` | Zoom Out (Time / Horizontal) |
| `Cmd + 0` | Reset Zoom to 100% |
| `Cmd + Shift + +` / `Cmd + Shift + =` | Increase Track Height (Vertical) |
| `Cmd + Shift + -` | Decrease Track Height (Vertical) |

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

# Run test suite (44 unit tests)
ctest --test-dir build -C Release --output-on-failure
```

### Launching

On macOS:
```bash
open build/src/TramiLoop_artefacts/Release/trami-loop.app
```

On Windows:
```cmd
.\build\src\Release\trami-loop.exe
```

---

## Architecture & Code Structure

The codebase is organized into clean, modular layers:
- `src/model/`: Pure C++ domain model (`Song`, `Pattern`, `Sample`, `Timing`, `UndoManager`). Zero GUI dependencies, completely unit-testable.
- `src/engine/`: Real-time audio engine running lock-free on the audio callback (`AudioEngine`, `Sequencer`, `SnapshotExchange`, `TimeStretcher`, `StretchCache`, `AudioRecorder`).
- `src/io/`: File and project I/O (`ProjectSerializer`, `SampleImporter`, `WavExporter`).
- `src/ui/`: JUCE Component hierarchy (`MainContainer`, `PatternView`, `PatternGrid`, `ArrangerView`, `ArrangerGrid`, `SampleBrowserPanel`, `TramiLookAndFeel`).
- `tests/`: Catch2 unit test suite covering musical timing math, model consistency, snapshot exchange, time stretching, serialization, and WAV export.

---

## License

This project is licensed under the GNU Affero General Public License v3 (AGPLv3).
The repository is maintained as a private repository.

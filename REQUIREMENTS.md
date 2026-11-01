# trami-loop — Requirements (v1)

## Vision
A very straightforward tool to make beats and arrangements. It combines the loop/one-shot grid with tempo-synced time stretching from **Acid Music** and the pattern → arrangement workflow from **Creator (Atari ST)**.

**Guiding principle:** pattern creation and arrangement must be as simple as possible.

## Platform & Tech
- Native standalone desktop app for **macOS and Windows**
- **C++ with JUCE** for performance and sample-accurate timing (all sequencing runs in the audio callback)
- **Rubber Band** library for time stretching
- **Open source, AGPLv3** (compatible with JUCE and Rubber Band)
- Code must be **very clean and maintainable**: clear module boundaries (engine / model / UI), no shortcuts on the audio thread
- MIDI is out of scope for v1, but the architecture should allow adding it later

## Song
- One **fixed BPM** per song
- One **global swing** factor for the whole song
- Master volume

## Samples
- Import formats: **WAV, MP3**. Files are automatically converted to the project sample rate
- Each sample is flagged as either:
  - **Loop**: you set its length in bars, and the original BPM is calculated from the file duration. It stretches to the song BPM
  - **One-shot**: plays unchanged, with no stretching
- Samples are **copied into the project folder**, so projects are self-contained
- Sources: a built-in **browser panel with preview**, plus **drag and drop** from Finder/Explorer

## Pattern View
- Each pattern has its own **length in bars**
- **16 tracks** per pattern. Tracks belong to that pattern only
- A track can hold **multiple clips** from any samples
- Grid resolution: **1/16**, with global swing applied
- Per track: **volume, pan**
- Clip editing (minimal): **place, move, delete**, plus copy/paste

## Arranger View
- Looks like the pattern view, but blocks are **patterns** instead of samples
- **8 arranger tracks**, so multiple patterns can play at the same time
- Per arranger track: **volume, pan, mute**
- Pattern management: create, duplicate, rename, delete

## Recording
- Record in the **pattern view**: creates a new sample and places it as a clip
- Record in the **arranger**: creates a new pattern containing the recording and places it on the track where you recorded
- Defaults: 1-bar count-in, automatic latency compensation, input device selected in preferences
- Metronome

## Transport
- Play, stop, return to start
- **Pattern mode** (loops the current pattern) and **song mode** (plays the arrangement)
- Metronome toggle
- Keyboard shortcuts (for example space to play/stop)

## Editing
- Undo/redo
- Copy/paste of clips and patterns

## Export
- **WAV mixdown of the whole song**
- 16-bit/44.1 kHz, 24-bit/44.1 kHz, 16-bit/48 kHz, 24-bit/48 kHz

## UI
- Super clean, **dark mode**, no clutter
- Colored blocks: **automatic color per sample**, with an indicator showing loop or one-shot

## Out of Scope (v1)
MIDI, effects, tempo changes, stem/pattern export, formats other than WAV/MP3, clip trim/pitch/reverse, arranger loop range, finer grid than 1/16.

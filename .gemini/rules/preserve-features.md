# Feature Preservation & Regression Prevention Rules

These rules apply to all development and modifications across the TramiLoop project. They are designed to prevent accidental breakage, loss of existing functionality, or unintended side effects when implementing new features or bug fixes.

---

## 1. Core Principle: Zero Unintended Breakage
* **Invariant Contract**: Existing functionality, UI elements, keyboard shortcuts, playback behavior, and file support must remain intact and functional unless the user explicitly requests their removal or modification.
* **Additive Changes**: Implement new features additively (new methods, optional flags, additional UI controls) rather than modifying or rewriting existing, working logic.
* **No Unsolicited Refactoring**: Avoid unsolicited architectural refactoring, cleanup, or "simplification" of existing code paths that are already working and tested.

---

## 2. Pre-Modification Impact Analysis
Before editing or removing existing code:
1. **Identify Existing Usages**: Search the codebase (`grep`, file references) for all callers and consumers of the classes, functions, or UI components being modified.
2. **Review Affected Tests**: Identify which unit tests verify the component or system.
3. **Verify Existing Behavior**: Ensure you understand how the current implementation works and why it was structured that way before altering it.

---

## 3. Test Verification & Invariance
* **All Tests Must Pass**: Run the full test suite (`ctest --test-dir build --output-on-failure`) before finishing any task. All existing tests must pass.
* **Never Weaken Existing Tests**: Do not delete, disable, or modify existing tests to accommodate new changes unless the user explicitly requested a change in that specific behavior. A failing existing test indicates a regression that must be resolved in the implementation.
* **Lock in Changes with New Tests**: Add automated unit tests for new features, format support, or bug fixes to prevent future regressions.

---

## 4. TramiLoop Specific Guardrails

### Audio Engine & Sequencer
* **Real-Time Safety**: Keep the audio thread lock-free and allocation-free. Do not introduce heap allocations (`new`, `std::vector` resizing), locks, or blocking system calls into `Sequencer::process` or voice rendering.
* **Separation of Concerns**:
  - Keep dedicated voices (such as preview and metronome) isolated from pattern track voices to prevent voice stealing or crosstalk.
  - The main volume meter and clip detection must reflect only the musical mix (tracks + preview) and never monitoring cues (metronome).
* **Playback & Timing Invariants**: Preserve timing accuracy (swing, sample-rate conversions, fractional steps, playhead follow, and looping bounds).

### Project & Media Compatibility
* **Dual Format Integrity**: Both native `.trami` projects and ACID projects (`.acd`, `.acd-zip`) must load and save seamlessly.
* **Path Resilience**: Always handle cross-platform path separators (normalizing Windows `\` to `/` on POSIX systems) and multi-strategy sample resolution (direct, case-insensitive, stem-based, and directory fallback).

### User Interface & Controls
* **Layout Stability**: Preserve existing view boundaries, toolbars, transport controls, channel strips, rulers, and zoom interactions when adding or repositioning UI elements.
* **Interactive Controls**: Ensure clip selection, dragging, slicing, edge resizing, and loop markers continue to function across all zoom levels and view modes.

---

## 5. Conflict Resolution Protocol
* If a new request appears to contradict or replace an existing feature, **do not assume** the existing feature should be discarded.
* Prompt the user for explicit clarification on whether to replace the old behavior or provide both (e.g., via a toggle or setting).


#pragma once

#include "engine/Sequencer.h"
#include "model/Pattern.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace trami
{

/// External VST3 / AudioUnit plugin host (zero built-in effects).
/// Manages plugin scanning, live plugin chains for Clips, Tracks, and Master Bus,
/// native plugin editor windows, and lock-free-friendly audio processing.
class PluginHost : public AudioPluginProcessor
{
public:
    enum class TargetKind
    {
        Clip,
        Track,
        Master
    };

    PluginHost();
    ~PluginHost() override;

    void prepareToPlay(double sampleRate, int maxBlockSize);
    void releaseResources();

    /// Known scanned VST3 / AU effect plugins.
    const juce::KnownPluginList& knownPlugins() const noexcept { return knownPlugins_; }
    juce::Array<juce::PluginDescription> availableEffectPlugins() const;

    /// Scans system VST3 / AU directories for audio effect plugins and caches the list.
    int scanDefaultPluginPaths(std::function<void(const juce::String&)> onStatus = nullptr);

    /// Synchronizes live plugin instances with the Song's current Pattern model.
    void syncFromPattern(const Pattern& pattern);

    /// Captures current Base64 state from live plugin instances back into the model's PluginSlot structs.
    void captureStatesIntoPattern(Pattern& pattern);

    /// Opens (or brings to front) the native VST3/AU GUI editor window for a hosted plugin slot.
    bool openPluginEditor(TargetKind kind, std::uint32_t targetId, int slotIndex);

    /// Closes all open plugin editor windows.
    void closeAllEditors();

    /// Clears all hosted plugin chains (e.g. when loading a new project).
    void clearAllChains();

    // --- AudioPluginProcessor (audio thread, non-blocking try-lock) ----------
    void processClip(std::uint32_t clipId, float* left, float* right, int numSamples) noexcept override;
    void processTrack(int trackIndex, float* left, float* right, int numSamples) noexcept override;
    void processMaster(float* left, float* right, int numSamples) noexcept override;

private:
    struct HostedSlot
    {
        std::string identifierString;
        bool bypassed = false;
        std::unique_ptr<juce::AudioPluginInstance> instance;
    };

    class EditorWindow;

    void loadPluginCache();
    void savePluginCache() const;
    juce::File pluginCacheFile() const;

    std::unique_ptr<juce::AudioPluginInstance> instantiateSlot(const PluginSlot& slot);
    void syncChain(std::vector<HostedSlot>& liveChain, const std::vector<PluginSlot>& modelChain);
    void captureChainStates(const std::vector<HostedSlot>& liveChain, std::vector<PluginSlot>& modelChain);
    void processChain(std::vector<HostedSlot>& chain, float* left, float* right, int numSamples) noexcept;

    juce::AudioPluginFormatManager formatManager_;
    juce::KnownPluginList knownPlugins_;

    double sampleRate_ = 44100.0;
    int maxBlockSize_ = 512;

    juce::SpinLock audioLock_;
    std::unordered_map<std::uint32_t, std::vector<HostedSlot>> clipChains_;
    std::unordered_map<int, std::vector<HostedSlot>> trackChains_;
    std::vector<HostedSlot> masterChain_;

    // Pre-allocated scratch buffer for AudioPluginInstance::processBlock (no allocations on audio thread)
    juce::AudioBuffer<float> scratchBuffer_;
    juce::MidiBuffer emptyMidi_;

    std::vector<std::unique_ptr<EditorWindow>> openEditors_;
};

} // namespace trami

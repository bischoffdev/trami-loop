#include "engine/AudioEngine.h"

namespace trami
{

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    stopTimer();
    deviceManager_.removeAudioCallback(this);
    deviceManager_.closeAudioDevice();
    sequencer_.reset(); // Audio has stopped, so its snapshots can be freed.
}

juce::String AudioEngine::start()
{
    const auto error = deviceManager_.initialiseWithDefaultDevices(0, 2);
    if (error.isNotEmpty())
        return error;

    auto* device = deviceManager_.getCurrentAudioDevice();
    if (device == nullptr)
        return "No audio output device available.";

    // The sequencer is created before the callback is attached, so the audio
    // thread never sees it half-constructed.
    sampleRate_ = device->getCurrentSampleRate();
    sequencer_ = std::make_unique<Sequencer>(exchange_, sampleRate_);
    deviceManager_.addAudioCallback(this);
    startTimerHz(10);
    return {};
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    // Sample-rate changes while running are handled in a later milestone.
    jassert(sampleRate_ == 0.0 || juce::approximatelyEqual(device->getCurrentSampleRate(), sampleRate_));
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const*, int, float* const* outputs, int numOutputs,
                                                   int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    for (int ch = 0; ch < numOutputs; ++ch)
        if (outputs[ch] != nullptr)
            juce::FloatVectorOperations::clear(outputs[ch], numSamples);

    if (numOutputs >= 2 && outputs[0] != nullptr && outputs[1] != nullptr)
        sequencer_->process(outputs[0], outputs[1], numSamples);
}

} // namespace trami

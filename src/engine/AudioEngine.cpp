#include "engine/AudioEngine.h"

namespace trami
{

AudioEngine::AudioEngine()
    : sampleRate_(44100.0)
    , sequencer_(std::make_unique<Sequencer>(exchange_, sampleRate_))
{
    sequencer_->setPluginProcessor(&pluginHost_);
}

AudioEngine::~AudioEngine()
{
    stopTimer();
    deviceManager_.removeAudioCallback(this);
    deviceManager_.closeAudioDevice();
    if (sequencer_ != nullptr)
        sequencer_->setPluginProcessor(nullptr);
    pluginHost_.closeAllEditors();
    pluginHost_.clearAllChains();
    sequencer_.reset();
}

juce::String AudioEngine::start()
{
    // Try to open with 2 inputs and 2 outputs; fall back to 0 inputs if no mic available
    auto error = deviceManager_.initialiseWithDefaultDevices(2, 2);
    if (error.isNotEmpty())
        error = deviceManager_.initialiseWithDefaultDevices(0, 2);
    if (error.isNotEmpty())
        return error;

    auto* device = deviceManager_.getCurrentAudioDevice();
    if (device == nullptr)
        return "No audio device available.";

    sampleRate_ = device->getCurrentSampleRate();
    sequencer_ = std::make_unique<Sequencer>(exchange_, sampleRate_);
    sequencer_->setPluginProcessor(&pluginHost_);
    pluginHost_.prepareToPlay(sampleRate_, device->getCurrentBufferSizeSamples());
    deviceManager_.addAudioCallback(this);
    startTimerHz(10);
    return {};
}

int AudioEngine::inputLatencySamples() const noexcept
{
    if (auto* device = deviceManager_.getCurrentAudioDevice())
        return device->getInputLatencyInSamples();
    return 0;
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    jassert(sampleRate_ == 0.0 || juce::approximatelyEqual(device->getCurrentSampleRate(), sampleRate_));
    if (device != nullptr)
        pluginHost_.prepareToPlay(device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void AudioEngine::audioDeviceStopped()
{
    pluginHost_.releaseResources();
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputs, int numInputs, float* const* outputs,
                                                   int numOutputs, int numSamples,
                                                   const juce::AudioIODeviceCallbackContext&)
{
    // Advance recorder count-in and process incoming inputs
    recorder_.advanceSamples(numSamples);
    if (numInputs > 0 && inputs != nullptr)
        recorder_.processInput(inputs, numInputs, numSamples);

    for (int ch = 0; ch < numOutputs; ++ch)
        if (outputs[ch] != nullptr)
            juce::FloatVectorOperations::clear(outputs[ch], numSamples);

    if (numOutputs >= 2 && outputs[0] != nullptr && outputs[1] != nullptr)
        sequencer_->process(outputs[0], outputs[1], numSamples);
}

} // namespace trami

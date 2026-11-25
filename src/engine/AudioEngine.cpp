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
    // Try to open with up to 32 inputs and 2 outputs; fall back to 2 inputs or 0 inputs if no mic available
    auto error = deviceManager_.initialiseWithDefaultDevices(32, 2);
    if (error.isNotEmpty())
        error = deviceManager_.initialiseWithDefaultDevices(2, 2);
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

int AudioEngine::outputLatencySamples() const noexcept
{
    if (auto* device = deviceManager_.getCurrentAudioDevice())
        return device->getOutputLatencyInSamples();
    return 0;
}

int AudioEngine::recordingLatencySamples() const noexcept
{
    const int pdcLat = (sequencer_ != nullptr) ? sequencer_->totalLatencySamples() : 0;
    return inputLatencySamples() + pdcLat;
}

int AudioEngine::numInputChannels() const noexcept
{
    if (auto* device = deviceManager_.getCurrentAudioDevice())
    {
        const int activeCount = device->getActiveInputChannels().countNumberOfSetBits();
        if (activeCount > 0)
            return std::min(activeCount, AudioRecorder::kMaxInputChannels);
        const int totalNames = device->getInputChannelNames().size();
        if (totalNames > 0)
            return std::min(totalNames, AudioRecorder::kMaxInputChannels);
    }
    const int recActive = recorder_.activeInputChannels();
    return std::max(2, recActive);
}

juce::StringArray AudioEngine::inputChannelNames() const
{
    juce::StringArray names;
    if (auto* device = deviceManager_.getCurrentAudioDevice())
    {
        const auto allNames = device->getInputChannelNames();
        const auto activeBits = device->getActiveInputChannels();
        if (activeBits.countNumberOfSetBits() > 0)
        {
            for (int i = 0; i < allNames.size() && names.size() < AudioRecorder::kMaxInputChannels; ++i)
            {
                if (activeBits[i])
                    names.add(allNames[i].isNotEmpty() ? allNames[i] : ("Input " + juce::String(i + 1)));
            }
        }
        else
        {
            for (int i = 0; i < allNames.size() && names.size() < AudioRecorder::kMaxInputChannels; ++i)
                names.add(allNames[i].isNotEmpty() ? allNames[i] : ("Input " + juce::String(i + 1)));
        }
    }
    if (names.isEmpty())
    {
        names.add("Input 1");
        names.add("Input 2");
    }
    return names;
}

std::pair<float, float> AudioEngine::inputPeakLevels(int inputChannel, bool stereo) const noexcept
{
    return recorder_.inputPeakLevels(inputChannel, stereo);
}

std::pair<float, float> AudioEngine::trackMeterLevels(const PatternTrack& track, int trackIndex) const noexcept
{
    auto [playL, playR] = (sequencer_ != nullptr)
        ? sequencer_->trackPeakLevels(trackIndex)
        : std::make_pair(0.0f, 0.0f);

    if (track.armed)
    {
        const auto [inL, inR] = recorder_.inputPeakLevels(track.inputChannel, track.inputStereo);
        const float gain = std::max(0.0f, track.mix.volume);
        playL = std::max(playL, inL * gain);
        playR = std::max(playR, inR * gain);
    }
    return {playL, playR};
}

AudioEngine::PerformanceMetrics AudioEngine::performanceMetrics() const noexcept
{
    PerformanceMetrics m;
    const float devCpu = static_cast<float>(deviceManager_.getCpuUsage());
    const float measCpu = dspLoad_.load(std::memory_order_relaxed);
    m.cpuLoad = std::clamp(std::max(devCpu, measCpu), 0.0f, 1.0f);

    if (sequencer_ != nullptr)
    {
        m.activeVoices = sequencer_->activeVoiceCount();
        m.pdcLatencySamples = sequencer_->totalLatencySamples();
    }

    const float voiceRatio = static_cast<float>(m.activeVoices) / 16.0f;
    const float peakCpu = peakDspLoad_.load(std::memory_order_relaxed);
    m.secondaryLoad = std::clamp(std::max(voiceRatio, peakCpu * 0.85f), 0.0f, 1.0f);
    return m;
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    jassert(sampleRate_ == 0.0 || juce::approximatelyEqual(device->getCurrentSampleRate(), sampleRate_));
    if (device != nullptr)
        pluginHost_.prepareToPlay(device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void AudioEngine::audioDeviceStopped()
{
    dspLoad_.store(0.0f, std::memory_order_relaxed);
    peakDspLoad_.store(0.0f, std::memory_order_relaxed);
    recorder_.clearInputPeaks();
    pluginHost_.releaseResources();
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputs, int numInputs, float* const* outputs,
                                                   int numOutputs, int numSamples,
                                                   const juce::AudioIODeviceCallbackContext&)
{
    const auto startTicks = juce::Time::getHighResolutionTicks();

    // Advance recorder count-in and process incoming inputs
    recorder_.advanceSamples(numSamples);
    if (numInputs > 0 && inputs != nullptr)
        recorder_.processInput(inputs, numInputs, numSamples);

    for (int ch = 0; ch < numOutputs; ++ch)
        if (outputs[ch] != nullptr)
            juce::FloatVectorOperations::clear(outputs[ch], numSamples);

    if (numOutputs >= 2 && outputs[0] != nullptr && outputs[1] != nullptr)
        sequencer_->process(outputs[0], outputs[1], numSamples);

    const auto endTicks = juce::Time::getHighResolutionTicks();
    const double elapsedSec = juce::Time::highResolutionTicksToSeconds(endTicks - startTicks);
    const double sr = std::max(8000.0, sampleRate_);
    const double budgetSec = static_cast<double>(std::max(1, numSamples)) / sr;
    const float blockLoad = std::clamp(static_cast<float>(elapsedSec / budgetSec), 0.0f, 1.0f);

    const float prevAvg = dspLoad_.load(std::memory_order_relaxed);
    const float nextAvg = (blockLoad > prevAvg)
        ? (prevAvg + 0.35f * (blockLoad - prevAvg))
        : (prevAvg * 0.92f + blockLoad * 0.08f);
    dspLoad_.store(nextAvg, std::memory_order_relaxed);

    const float prevPeak = peakDspLoad_.load(std::memory_order_relaxed);
    const float nextPeak = std::max(blockLoad, prevPeak * 0.96f);
    peakDspLoad_.store(nextPeak, std::memory_order_relaxed);
}

} // namespace trami

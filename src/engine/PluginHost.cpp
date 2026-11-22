#include "engine/PluginHost.h"

#include <algorithm>
#include <cstring>

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace trami
{

namespace
{
#if JUCE_WINDOWS
// Plain function with no C++ objects requiring stack unwinding so MSVC allows __try/__except (no C2712).
bool scanNextFileProtectedSEH(juce::PluginDirectoryScanner* scanner, juce::String* outName, bool* outHasMore)
{
    const UINT prevMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    bool ok = true;
    __try
    {
        *outHasMore = scanner->scanNextFile(true, *outName);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ok = false;
    }
    SetErrorMode(prevMode);
    return ok;
}
#endif
} // namespace

class PluginHost::EditorWindow final : public juce::DocumentWindow
{
public:
    EditorWindow(const juce::String& title,
                 juce::AudioProcessorEditor* editor,
                 juce::AudioPluginInstance* ownerInstance,
                 std::function<void(EditorWindow*)> onClose)
        : juce::DocumentWindow(title, juce::Colour(0xff1e1e24), juce::DocumentWindow::closeButton),
          ownerInstance_(ownerInstance),
          onClose_(std::move(onClose))
    {
        setUsingNativeTitleBar(true);
        setContentOwned(editor, true);
        setResizable(editor->isResizable(), false);
        setAlwaysOnTop(true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
        toFront(true);
    }

    juce::AudioPluginInstance* ownerInstance() const noexcept { return ownerInstance_; }

    void closeButtonPressed() override
    {
        if (onClose_)
            onClose_(this);
    }

private:
    juce::AudioPluginInstance* ownerInstance_ = nullptr;
    std::function<void(EditorWindow*)> onClose_;
};

PluginHost::PluginHost()
{
    formatManager_.addDefaultFormats();
    scratchBuffer_.setSize(16, 4096, false, true, false);
    loadPluginCache();
}

PluginHost::~PluginHost()
{
    closeAllEditors();
    clearAllChains();
}

void PluginHost::prepareToPlay(double sampleRate, int maxBlockSize)
{
    const juce::SpinLock::ScopedLockType lock(audioLock_);
    sampleRate_ = std::max(8000.0, sampleRate);
    maxBlockSize_ = std::clamp(maxBlockSize, 64, 4096);
    if (scratchBuffer_.getNumChannels() < 16 || scratchBuffer_.getNumSamples() < maxBlockSize_)
        scratchBuffer_.setSize(std::max(16, scratchBuffer_.getNumChannels()), std::max(4096, maxBlockSize_), false, true, false);

    auto prepChain = [&](std::vector<HostedSlot>& chain) {
        for (auto& s : chain)
        {
            if (s.instance != nullptr)
            {
                s.instance->setRateAndBufferSizeDetails(sampleRate_, maxBlockSize_);
                s.instance->prepareToPlay(sampleRate_, maxBlockSize_);
            }
        }
    };

    for (auto& [id, ch] : clipChains_)
        prepChain(ch);
    for (auto& [t, ch] : trackChains_)
        prepChain(ch);
    prepChain(masterChain_);
}

void PluginHost::releaseResources()
{
    const juce::SpinLock::ScopedLockType lock(audioLock_);
    auto relChain = [](std::vector<HostedSlot>& chain) {
        for (auto& s : chain)
            if (s.instance != nullptr)
                s.instance->releaseResources();
    };
    for (auto& [id, ch] : clipChains_)
        relChain(ch);
    for (auto& [t, ch] : trackChains_)
        relChain(ch);
    relChain(masterChain_);
}

juce::Array<juce::PluginDescription> PluginHost::availableEffectPlugins() const
{
    juce::Array<juce::PluginDescription> result;
    for (const auto& desc : knownPlugins_.getTypes())
    {
        if (!desc.isInstrument)
            result.add(desc);
    }
    return result;
}

juce::File PluginHost::pluginCacheFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("trami-loop")
        .getChildFile("known_plugins.xml");
}

void PluginHost::loadPluginCache()
{
    const auto f = pluginCacheFile();
    if (f.existsAsFile())
    {
        if (auto xml = juce::parseXML(f))
            knownPlugins_.recreateFromXml(*xml);
    }
}

void PluginHost::savePluginCache() const
{
    const auto f = pluginCacheFile();
    f.getParentDirectory().createDirectory();
    if (auto xml = knownPlugins_.createXml())
        xml->writeTo(f);
}

void PluginHost::beginDefaultPluginScan()
{
    activeScanner_.reset();
    scanFormatIndex_ = 0;
    scanSkippedCount_ = 0;
    scanScannedCount_ = 0;
    scanActive_ = true;

    const auto cacheDir = pluginCacheFile().getParentDirectory();
    cacheDir.createDirectory();

    // If a previous scan hard-crashed on a rogue plugin, blacklist it from the dead-man's-pedal file
    const auto deadMansPedal = cacheDir.getChildFile("plugin_scan_crash.txt");
    if (deadMansPedal.existsAsFile())
    {
        juce::StringArray crashedFiles;
        deadMansPedal.readLines(crashedFiles);
        for (const auto& line : crashedFiles)
        {
            const auto trimmed = line.trim();
            if (trimmed.isNotEmpty())
                knownPlugins_.addToBlacklist(trimmed);
        }
        deadMansPedal.deleteFile();
        savePluginCache();
    }
}

bool PluginHost::scanNextPlugin(juce::String& outStatus, int& outSkippedCount)
{
    if (!scanActive_)
        return false;

    const auto deadMansPedal = pluginCacheFile().getParentDirectory().getChildFile("plugin_scan_crash.txt");

    // Advance to next format scanner if needed
    while (activeScanner_ == nullptr)
    {
        if (scanFormatIndex_ >= formatManager_.getNumFormats())
        {
            outSkippedCount = scanSkippedCount_;
            return false;
        }

        auto* format = formatManager_.getFormat(scanFormatIndex_++);
        if (format == nullptr)
            continue;

        const auto paths = format->getDefaultLocationsToSearch();
        activeScanner_ = std::make_unique<juce::PluginDirectoryScanner>(
            knownPlugins_, *format, paths, true, deadMansPedal, false);
    }

    const juce::String nextFile = activeScanner_->getNextPluginFileThatWillBeScanned();
    if (nextFile.isEmpty())
    {
        activeScanner_.reset();
        outSkippedCount = scanSkippedCount_;
        return scanFormatIndex_ < formatManager_.getNumFormats();
    }

    juce::String pluginBeingScanned;
    bool hasMore = false;
    bool ok = true;

#if JUCE_WINDOWS
    ok = scanNextFileProtectedSEH(activeScanner_.get(), &pluginBeingScanned, &hasMore);
#else
    try
    {
        hasMore = activeScanner_->scanNextFile(true, pluginBeingScanned);
    }
    catch (...)
    {
        ok = false;
    }
#endif

    if (!ok)
    {
        ++scanSkippedCount_;
        if (nextFile.isNotEmpty())
            knownPlugins_.addToBlacklist(nextFile);
        activeScanner_->skipNextFile();
        outStatus = "Skipped crashing plugin: " + juce::File(nextFile).getFileName();
        savePluginCache();
        outSkippedCount = scanSkippedCount_;
        return true;
    }

    ++scanScannedCount_;
    if (pluginBeingScanned.isNotEmpty())
        outStatus = "Scanning: " + pluginBeingScanned;
    else
        outStatus = "Scanning: " + juce::File(nextFile).getFileName();

    if ((scanScannedCount_ % 5) == 0)
        savePluginCache();

    if (!hasMore)
        activeScanner_.reset();

    outSkippedCount = scanSkippedCount_;
    return (activeScanner_ != nullptr) || (scanFormatIndex_ < formatManager_.getNumFormats());
}

int PluginHost::finishPluginScan()
{
    activeScanner_.reset();
    scanActive_ = false;
    const auto deadMansPedal = pluginCacheFile().getParentDirectory().getChildFile("plugin_scan_crash.txt");
    if (deadMansPedal.existsAsFile())
        deadMansPedal.deleteFile();
    savePluginCache();
    return availableEffectPlugins().size();
}

int PluginHost::scanDefaultPluginPaths(std::function<void(const juce::String&)> onStatus)
{
    beginDefaultPluginScan();
    juce::String status;
    int skipped = 0;
    while (scanNextPlugin(status, skipped))
    {
        if (onStatus && status.isNotEmpty())
            onStatus(status);
    }
    return finishPluginScan();
}

std::unique_ptr<juce::AudioPluginInstance> PluginHost::instantiateSlot(const PluginSlot& slot)
{
    juce::PluginDescription desc;
    bool matched = false;

    for (const auto& known : knownPlugins_.getTypes())
    {
        if (!slot.identifierString.empty() && known.createIdentifierString().toStdString() == slot.identifierString)
        {
            desc = known;
            matched = true;
            break;
        }
        if (!slot.fileOrIdentifier.empty() && known.fileOrIdentifier.toStdString() == slot.fileOrIdentifier &&
            known.name.toStdString() == slot.name)
        {
            desc = known;
            matched = true;
            break;
        }
    }

    if (!matched)
    {
        desc.name = juce::String::fromUTF8(slot.name.c_str());
        desc.pluginFormatName = juce::String::fromUTF8(slot.format.c_str());
        desc.fileOrIdentifier = juce::String::fromUTF8(slot.fileOrIdentifier.c_str());
    }

    juce::String errorMessage;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    try
    {
        instance = formatManager_.createPluginInstance(desc, sampleRate_, maxBlockSize_, errorMessage);
    }
    catch (...)
    {
        return nullptr;
    }

    if (instance != nullptr)
    {
        try
        {
            // Configure stereo main input/output layout and disable unused aux/sidechain buses if supported
            auto layout = instance->getBusesLayout();
            if (!layout.inputBuses.isEmpty())
            {
                layout.inputBuses.getReference(0) = juce::AudioChannelSet::stereo();
                for (int b = 1; b < layout.inputBuses.size(); ++b)
                    layout.inputBuses.getReference(b) = juce::AudioChannelSet::disabled();
            }
            if (!layout.outputBuses.isEmpty())
            {
                layout.outputBuses.getReference(0) = juce::AudioChannelSet::stereo();
                for (int b = 1; b < layout.outputBuses.size(); ++b)
                    layout.outputBuses.getReference(b) = juce::AudioChannelSet::disabled();
            }
            if (instance->checkBusesLayoutSupported(layout))
                instance->setBusesLayout(layout);

            instance->setRateAndBufferSizeDetails(sampleRate_, maxBlockSize_);
            instance->prepareToPlay(sampleRate_, maxBlockSize_);

            const int reqCh = std::max({2, instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels()});
            if (reqCh > scratchBuffer_.getNumChannels())
            {
                const juce::SpinLock::ScopedLockType lock(audioLock_);
                if (reqCh > scratchBuffer_.getNumChannels())
                    scratchBuffer_.setSize(reqCh, std::max(4096, maxBlockSize_), false, true, false);
            }

            if (!slot.stateBase64.empty())
            {
                juce::MemoryBlock stateBlock;
                if (stateBlock.fromBase64Encoding(juce::String::fromUTF8(slot.stateBase64.c_str())))
                    instance->setStateInformation(stateBlock.getData(), static_cast<int>(stateBlock.getSize()));
            }
        }
        catch (...)
        {
            return nullptr;
        }
    }
    return instance;
}

void PluginHost::closeEditorsForChain(const std::vector<HostedSlot>& retiredChain)
{
    for (const auto& oldSlot : retiredChain)
    {
        if (oldSlot.instance != nullptr)
        {
            openEditors_.erase(
                std::remove_if(openEditors_.begin(), openEditors_.end(),
                               [&](const std::unique_ptr<EditorWindow>& w) {
                                   return w != nullptr && w->ownerInstance() == oldSlot.instance.get();
                               }),
                openEditors_.end());
        }
    }
}

std::vector<PluginHost::HostedSlot> PluginHost::buildNextChain(
    const std::vector<HostedSlot>& currentChain, const std::vector<PluginSlot>& modelChain,
    std::vector<std::unique_ptr<juce::AudioPluginInstance>>& newlyCreated)
{
    // Count which keys already exist in currentChain so we only instantiate brand-new slots outside audioLock_
    std::vector<std::string> availableKeys;
    availableKeys.reserve(currentChain.size());
    for (const auto& s : currentChain)
        if (s.instance != nullptr)
            availableKeys.push_back(s.identifierString);

    std::vector<HostedSlot> nextChain;
    nextChain.reserve(modelChain.size());
    newlyCreated.clear();

    for (const auto& mSlot : modelChain)
    {
        const std::string key = !mSlot.identifierString.empty()
                                    ? mSlot.identifierString
                                    : (mSlot.format + ":" + mSlot.fileOrIdentifier + ":" + mSlot.name);

        HostedSlot hosted;
        hosted.identifierString = key;
        hosted.bypassed = mSlot.bypassed;

        auto it = std::find(availableKeys.begin(), availableKeys.end(), key);
        if (it != availableKeys.end())
        {
            availableKeys.erase(it);
            // Leave hosted.instance nullptr as a marker to steal the matching live instance under audioLock_
        }
        else
        {
            hosted.instance = instantiateSlot(mSlot);
        }

        nextChain.push_back(std::move(hosted));
    }
    return nextChain;
}

void PluginHost::syncMasterChain(const std::vector<PluginSlot>& modelChain)
{
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> dummy;
    auto nextChain = buildNextChain(masterChain_, modelChain, dummy);
    std::vector<HostedSlot> retired;

    {
        const juce::SpinLock::ScopedLockType lock(audioLock_);
        for (auto& slot : nextChain)
        {
            if (slot.instance == nullptr)
            {
                for (auto& existing : masterChain_)
                {
                    if (existing.instance != nullptr && existing.identifierString == slot.identifierString)
                    {
                        slot.instance = std::move(existing.instance);
                        break;
                    }
                }
            }
        }
        retired = std::move(masterChain_);
        masterChain_ = std::move(nextChain);
    }

    // Close editors & destroy retired instances completely outside audioLock_
    closeEditorsForChain(retired);
}

void PluginHost::syncTrackChain(int trackIndex, const std::vector<PluginSlot>& modelChain)
{
    std::vector<HostedSlot> emptyChain;
    const auto existingIt = trackChains_.find(trackIndex);
    const auto& currentRef = (existingIt != trackChains_.end()) ? existingIt->second : emptyChain;

    std::vector<std::unique_ptr<juce::AudioPluginInstance>> dummy;
    auto nextChain = buildNextChain(currentRef, modelChain, dummy);
    std::vector<HostedSlot> retired;

    {
        const juce::SpinLock::ScopedLockType lock(audioLock_);
        auto it = trackChains_.find(trackIndex);
        if (it != trackChains_.end())
        {
            for (auto& slot : nextChain)
            {
                if (slot.instance == nullptr)
                {
                    for (auto& existing : it->second)
                    {
                        if (existing.instance != nullptr && existing.identifierString == slot.identifierString)
                        {
                            slot.instance = std::move(existing.instance);
                            break;
                        }
                    }
                }
            }
            retired = std::move(it->second);
            if (nextChain.empty())
                trackChains_.erase(it);
            else
                it->second = std::move(nextChain);
        }
        else if (!nextChain.empty())
        {
            trackChains_.emplace(trackIndex, std::move(nextChain));
        }
    }

    closeEditorsForChain(retired);
}

void PluginHost::syncClipChain(std::uint32_t clipId, const std::vector<PluginSlot>& modelChain)
{
    std::vector<HostedSlot> emptyChain;
    const auto existingIt = clipChains_.find(clipId);
    const auto& currentRef = (existingIt != clipChains_.end()) ? existingIt->second : emptyChain;

    std::vector<std::unique_ptr<juce::AudioPluginInstance>> dummy;
    auto nextChain = buildNextChain(currentRef, modelChain, dummy);
    std::vector<HostedSlot> retired;

    {
        const juce::SpinLock::ScopedLockType lock(audioLock_);
        auto it = clipChains_.find(clipId);
        if (it != clipChains_.end())
        {
            for (auto& slot : nextChain)
            {
                if (slot.instance == nullptr)
                {
                    for (auto& existing : it->second)
                    {
                        if (existing.instance != nullptr && existing.identifierString == slot.identifierString)
                        {
                            slot.instance = std::move(existing.instance);
                            break;
                        }
                    }
                }
            }
            retired = std::move(it->second);
            if (nextChain.empty())
                clipChains_.erase(it);
            else
                it->second = std::move(nextChain);
        }
        else if (!nextChain.empty())
        {
            clipChains_.emplace(clipId, std::move(nextChain));
        }
    }

    closeEditorsForChain(retired);
}

void PluginHost::syncFromPattern(const Pattern& pattern)
{
    // 1. Master chain
    syncMasterChain(pattern.masterFxChain);

    // 2. Track chains (sync active tracks + prune any removed higher-index tracks)
    const int numTracks = static_cast<int>(pattern.tracks.size());
    std::vector<int> staleTracks;
    for (const auto& [t, ch] : trackChains_)
        if (t < 0 || t >= numTracks)
            staleTracks.push_back(t);
    for (int t : staleTracks)
        syncTrackChain(t, {});

    for (int t = 0; t < numTracks; ++t)
        syncTrackChain(t, pattern.tracks[static_cast<std::size_t>(t)].fxChain);

    // 3. Clip chains
    std::unordered_map<std::uint32_t, const std::vector<PluginSlot>*> activeClips;
    for (const auto& trk : pattern.tracks)
    {
        for (const auto& c : trk.clips)
        {
            if (!c.fxChain.empty())
                activeClips[c.id.value] = &c.fxChain;
        }
    }

    std::vector<std::uint32_t> staleClipIds;
    for (const auto& [cid, ch] : clipChains_)
    {
        if (activeClips.find(cid) == activeClips.end())
            staleClipIds.push_back(cid);
    }
    for (auto cid : staleClipIds)
        syncClipChain(cid, {});

    for (const auto& [cid, mChain] : activeClips)
        syncClipChain(cid, *mChain);
}

void PluginHost::captureChainStates(const std::vector<HostedSlot>& liveChain, std::vector<PluginSlot>& modelChain)
{
    const std::size_t n = std::min(liveChain.size(), modelChain.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        if (liveChain[i].instance != nullptr)
        {
            juce::MemoryBlock stateBlock;
            liveChain[i].instance->getStateInformation(stateBlock);
            modelChain[i].stateBase64 = stateBlock.toBase64Encoding().toStdString();
        }
    }
}

void PluginHost::captureStatesIntoPattern(Pattern& pattern)
{
    captureChainStates(masterChain_, pattern.masterFxChain);
    for (std::size_t t = 0; t < pattern.tracks.size(); ++t)
    {
        if (auto it = trackChains_.find(static_cast<int>(t)); it != trackChains_.end())
            captureChainStates(it->second, pattern.tracks[t].fxChain);
        for (auto& c : pattern.tracks[t].clips)
        {
            if (auto it = clipChains_.find(c.id.value); it != clipChains_.end())
                captureChainStates(it->second, c.fxChain);
        }
    }
}

bool PluginHost::openPluginEditor(TargetKind kind, std::uint32_t targetId, int slotIndex)
{
    std::vector<HostedSlot>* chain = nullptr;
    if (kind == TargetKind::Master)
        chain = &masterChain_;
    else if (kind == TargetKind::Track)
    {
        auto it = trackChains_.find(static_cast<int>(targetId));
        if (it != trackChains_.end())
            chain = &it->second;
    }
    else if (kind == TargetKind::Clip)
    {
        auto it = clipChains_.find(targetId);
        if (it != clipChains_.end())
            chain = &it->second;
    }

    if (chain == nullptr || slotIndex < 0 || slotIndex >= static_cast<int>(chain->size()))
        return false;

    auto* instance = (*chain)[static_cast<std::size_t>(slotIndex)].instance.get();
    if (instance == nullptr)
        return false;

    // Bring existing window to front if already open
    for (auto& w : openEditors_)
    {
        if (w != nullptr && w->ownerInstance() == instance)
        {
            w->toFront(true);
            return true;
        }
    }

    juce::AudioProcessorEditor* editor = instance->createEditorIfNeeded();
    if (editor == nullptr)
        editor = new juce::GenericAudioProcessorEditor(*instance);

    auto win = std::make_unique<EditorWindow>(
        instance->getName(),
        editor,
        instance,
        [this](EditorWindow* toClose) {
            openEditors_.erase(
                std::remove_if(openEditors_.begin(), openEditors_.end(),
                               [toClose](const std::unique_ptr<EditorWindow>& w) { return w.get() == toClose; }),
                openEditors_.end());
        });
    openEditors_.push_back(std::move(win));
    return true;
}

void PluginHost::closeAllEditors()
{
    openEditors_.clear();
}

void PluginHost::clearAllChains()
{
    closeAllEditors();
    const juce::SpinLock::ScopedLockType lock(audioLock_);
    clipChains_.clear();
    trackChains_.clear();
    masterChain_.clear();
}

void PluginHost::processChain(std::vector<HostedSlot>& chain, float* left, float* right, int numSamples) noexcept
{
    if (chain.empty() || numSamples <= 0)
        return;

    const int n = std::min(numSamples, scratchBuffer_.getNumSamples());
    if (n <= 0 || scratchBuffer_.getNumChannels() < 2)
        return;

    std::memcpy(scratchBuffer_.getWritePointer(0), left, static_cast<std::size_t>(n) * sizeof(float));
    std::memcpy(scratchBuffer_.getWritePointer(1), right, static_cast<std::size_t>(n) * sizeof(float));

    emptyMidi_.clear();

    bool anyProcessed = false;
    for (auto& slot : chain)
    {
        if (!slot.bypassed && slot.instance != nullptr)
        {
            const int inCh = slot.instance->getTotalNumInputChannels();
            const int outCh = slot.instance->getTotalNumOutputChannels();
            const int reqCh = std::max({2, inCh, outCh});
            if (reqCh > scratchBuffer_.getNumChannels())
                continue;

            // Zero any auxiliary/sidechain channels above stereo
            for (int ch = 2; ch < reqCh; ++ch)
                std::memset(scratchBuffer_.getWritePointer(ch), 0, static_cast<std::size_t>(n) * sizeof(float));

            juce::AudioBuffer<float> view(scratchBuffer_.getArrayOfWritePointers(), reqCh, n);
            slot.instance->processBlock(view, emptyMidi_);

            if (outCh == 1)
                std::memcpy(scratchBuffer_.getWritePointer(1), scratchBuffer_.getReadPointer(0),
                            static_cast<std::size_t>(n) * sizeof(float));

            anyProcessed = true;
        }
    }

    if (anyProcessed)
    {
        std::memcpy(left, scratchBuffer_.getReadPointer(0), static_cast<std::size_t>(n) * sizeof(float));
        std::memcpy(right, scratchBuffer_.getReadPointer(1), static_cast<std::size_t>(n) * sizeof(float));
    }
}

void PluginHost::processClip(std::uint32_t clipId, float* left, float* right, int numSamples) noexcept
{
    const juce::SpinLock::ScopedTryLockType tryLock(audioLock_);
    if (!tryLock.isLocked())
        return;
    auto it = clipChains_.find(clipId);
    if (it != clipChains_.end())
        processChain(it->second, left, right, numSamples);
}

void PluginHost::processTrack(int trackIndex, float* left, float* right, int numSamples) noexcept
{
    const juce::SpinLock::ScopedTryLockType tryLock(audioLock_);
    if (!tryLock.isLocked())
        return;
    auto it = trackChains_.find(trackIndex);
    if (it != trackChains_.end())
        processChain(it->second, left, right, numSamples);
}

void PluginHost::processMaster(float* left, float* right, int numSamples) noexcept
{
    const juce::SpinLock::ScopedTryLockType tryLock(audioLock_);
    if (!tryLock.isLocked())
        return;
    processChain(masterChain_, left, right, numSamples);
}

} // namespace trami

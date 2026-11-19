#include "engine/PluginHost.h"

#include <algorithm>
#include <cstring>

namespace trami
{

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
    scratchBuffer_.setSize(2, 4096, false, true, false);
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
    if (scratchBuffer_.getNumSamples() < maxBlockSize_)
        scratchBuffer_.setSize(2, maxBlockSize_, false, true, false);

    auto prepChain = [&](std::vector<HostedSlot>& chain) {
        for (auto& s : chain)
        {
            if (s.instance != nullptr)
            {
                s.instance->setPlayConfigDetails(2, 2, sampleRate_, maxBlockSize_);
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

int PluginHost::scanDefaultPluginPaths(std::function<void(const juce::String&)> onStatus)
{
    int foundCount = 0;
    const auto deadMansPedal = pluginCacheFile().getParentDirectory().getChildFile("plugin_scan_crash.txt");

    for (int i = 0; i < formatManager_.getNumFormats(); ++i)
    {
        auto* format = formatManager_.getFormat(i);
        if (format == nullptr)
            continue;

        const auto paths = format->getDefaultLocationsToSearch();
        juce::PluginDirectoryScanner scanner(knownPlugins_, *format, paths, true, deadMansPedal, false);
        juce::String pluginBeingScanned;
        while (scanner.scanNextFile(true, pluginBeingScanned))
        {
            if (onStatus && pluginBeingScanned.isNotEmpty())
                onStatus(format->getName() + ": " + pluginBeingScanned);
        }
    }

    foundCount = availableEffectPlugins().size();
    savePluginCache();
    return foundCount;
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
    auto instance = formatManager_.createPluginInstance(desc, sampleRate_, maxBlockSize_, errorMessage);
    if (instance != nullptr)
    {
        instance->setPlayConfigDetails(2, 2, sampleRate_, maxBlockSize_);
        instance->prepareToPlay(sampleRate_, maxBlockSize_);
        if (!slot.stateBase64.empty())
        {
            juce::MemoryBlock stateBlock;
            if (stateBlock.fromBase64Encoding(juce::String::fromUTF8(slot.stateBase64.c_str())))
                instance->setStateInformation(stateBlock.getData(), static_cast<int>(stateBlock.getSize()));
        }
    }
    return instance;
}

void PluginHost::syncChain(std::vector<HostedSlot>& liveChain, const std::vector<PluginSlot>& modelChain)
{
    std::vector<HostedSlot> nextChain;
    nextChain.reserve(modelChain.size());

    // Reuse existing live instances where identifier matches so plugin state & open UI windows stay intact
    for (const auto& mSlot : modelChain)
    {
        const std::string key = !mSlot.identifierString.empty()
                                    ? mSlot.identifierString
                                    : (mSlot.format + ":" + mSlot.fileOrIdentifier + ":" + mSlot.name);

        HostedSlot hosted;
        hosted.identifierString = key;
        hosted.bypassed = mSlot.bypassed;

        for (auto& existing : liveChain)
        {
            if (existing.instance != nullptr && existing.identifierString == key)
            {
                hosted.instance = std::move(existing.instance);
                break;
            }
        }

        if (hosted.instance == nullptr)
            hosted.instance = instantiateSlot(mSlot);

        nextChain.push_back(std::move(hosted));
    }

    // Close any open editor windows whose plugin instance is being retired
    for (const auto& oldSlot : liveChain)
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

    const juce::SpinLock::ScopedLockType lock(audioLock_);
    liveChain = std::move(nextChain);
}

void PluginHost::syncFromPattern(const Pattern& pattern)
{
    // 1. Master chain
    syncChain(masterChain_, pattern.masterFxChain);

    // 2. Track chains
    const int numTracks = static_cast<int>(pattern.tracks.size());
    for (int t = 0; t < numTracks; ++t)
    {
        const auto& trkChain = pattern.tracks[static_cast<std::size_t>(t)].fxChain;
        if (trkChain.empty())
        {
            auto it = trackChains_.find(t);
            if (it != trackChains_.end())
            {
                syncChain(it->second, {});
                const juce::SpinLock::ScopedLockType lock(audioLock_);
                trackChains_.erase(it);
            }
        }
        else
        {
            syncChain(trackChains_[t], trkChain);
        }
    }

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

    // Remove chains for deleted/cleared clips
    std::vector<std::uint32_t> staleClipIds;
    for (const auto& [cid, ch] : clipChains_)
    {
        if (activeClips.find(cid) == activeClips.end())
            staleClipIds.push_back(cid);
    }
    for (auto cid : staleClipIds)
    {
        syncChain(clipChains_[cid], {});
        const juce::SpinLock::ScopedLockType lock(audioLock_);
        clipChains_.erase(cid);
    }

    for (const auto& [cid, mChain] : activeClips)
        syncChain(clipChains_[cid], *mChain);
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
    std::memcpy(scratchBuffer_.getWritePointer(0), left, static_cast<std::size_t>(n) * sizeof(float));
    std::memcpy(scratchBuffer_.getWritePointer(1), right, static_cast<std::size_t>(n) * sizeof(float));

    juce::AudioBuffer<float> view(scratchBuffer_.getArrayOfWritePointers(), 2, n);
    emptyMidi_.clear();

    bool anyProcessed = false;
    for (auto& slot : chain)
    {
        if (!slot.bypassed && slot.instance != nullptr)
        {
            slot.instance->processBlock(view, emptyMidi_);
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

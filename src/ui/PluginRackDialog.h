#pragma once

#include "engine/PluginHost.h"
#include "model/Pattern.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace trami
{

struct PluginRackNavActions
{
    juce::String primaryLabel;
    std::function<void()> onPrimary;
    juce::String secondaryLabel;
    std::function<void()> onSecondary;
    juce::Colour accentColour{0xff5ac8fa};
    juce::String targetBanner;
};

/// Modeless VST3/AU FX Rack dialog for a Clip, Track, or Master Bus.
class PluginRackDialog
{
public:
    using NavigationActions = PluginRackNavActions;

    static void showAsync(const juce::String& title, PluginHost& pluginHost, PluginHost::TargetKind targetKind,
                          std::uint32_t targetId, std::vector<PluginSlot> initialChain,
                          std::function<void(std::vector<PluginSlot>)> onChainChanged,
                          NavigationActions navActions = {});
};

} // namespace trami

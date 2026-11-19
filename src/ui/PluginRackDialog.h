#pragma once

#include "engine/PluginHost.h"
#include "model/Pattern.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace trami
{

/// Modeless VST3/AU FX Rack dialog for a Clip, Track, or Master Bus.
class PluginRackDialog
{
public:
    static void showAsync(const juce::String& title, PluginHost& pluginHost, PluginHost::TargetKind targetKind,
                          std::uint32_t targetId, std::vector<PluginSlot> initialChain,
                          std::function<void(std::vector<PluginSlot>)> onChainChanged);
};

} // namespace trami

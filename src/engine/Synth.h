#pragma once

#include "engine/AudioData.h"

namespace trami::synth
{

/// Short sine blip used by the metronome.
AudioData click(double sampleRate, double frequency);

/// Simple synthesized drum sounds, used as built-in demo content.
AudioData kick(double sampleRate);
AudioData snare(double sampleRate);
AudioData hihat(double sampleRate);

/// A 1-bar bassline loop at 120 BPM, used to test time-stretching.
AudioData bassLoop(double sampleRate);

} // namespace trami::synth

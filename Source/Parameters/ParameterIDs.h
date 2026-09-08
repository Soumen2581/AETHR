#pragma once

/**
    Stable parameter identifiers.

    THESE STRINGS ARE PART OF THE PUBLIC CONTRACT WITH EVERY HOST PROJECT EVER
    SAVED. Renaming one silently breaks automation and saved state in users'
    sessions. Rules:

      - IDs are lowercase, dot-separated, and namespaced by subsystem.
      - An ID is never reused for a different meaning.
      - A retired parameter's ID stays retired; add a new one instead.
      - `versionHint` must stay 1 for every parameter shipped in the 1.x series.
*/
namespace aethr::params
{

inline constexpr int versionHint = 1;

namespace output
{
    inline constexpr auto gain    = "output.gain";
    inline constexpr auto ceiling = "output.ceiling";
}

namespace master
{
    inline constexpr auto tuneOctave    = "master.tune.octave";
    inline constexpr auto tuneSemitones = "master.tune.semitones";
    inline constexpr auto tuneCents     = "master.tune.cents";
}

namespace voice
{
    inline constexpr auto polyphony      = "voice.polyphony";
    inline constexpr auto velocityAmount = "voice.velocity.amount";
}

namespace engine
{
    inline constexpr auto type     = "engine.type";
    inline constexpr auto controlA = "engine.control.a";
    inline constexpr auto controlB = "engine.control.b";
    inline constexpr auto controlC = "engine.control.c";
    inline constexpr auto controlD = "engine.control.d";
}

namespace arp
{
    inline constexpr auto enable   = "arp.enable";
    inline constexpr auto mode     = "arp.mode";
    inline constexpr auto division = "arp.division";
    inline constexpr auto octaves  = "arp.octaves";
    inline constexpr auto gate     = "arp.gate";
    inline constexpr auto swing    = "arp.swing";
    inline constexpr auto latch    = "arp.latch";
    inline constexpr auto pattern  = "arp.pattern";
}

namespace resonator
{
    inline constexpr auto decayTime        = "resonator.decay.time";
    inline constexpr auto releaseTime      = "resonator.release.time";
    inline constexpr auto damping          = "resonator.damping";
    inline constexpr auto brightness       = "resonator.brightness";
    inline constexpr auto loopFilterMode   = "resonator.loopfilter.mode";
    inline constexpr auto interpolation    = "resonator.interpolation";
    inline constexpr auto stiffness        = "resonator.stiffness";
    inline constexpr auto feedback         = "resonator.feedback";
}

namespace exciter
{
    inline constexpr auto type          = "exciter.type";
    inline constexpr auto burstTime     = "exciter.burst.time";
    inline constexpr auto attack        = "exciter.attack";
    inline constexpr auto colour        = "exciter.colour";
    inline constexpr auto brightness    = "exciter.brightness";
    inline constexpr auto level         = "exciter.level";
    inline constexpr auto randomAmount  = "exciter.random.amount";
    inline constexpr auto seedLocked    = "exciter.seed.locked";
    inline constexpr auto seed          = "exciter.seed";
    inline constexpr auto stereoSpread  = "exciter.stereo.spread";
}

namespace material
{
    inline constexpr auto type = "material.type";
}

namespace body
{
    inline constexpr auto mix      = "body.mix";
    inline constexpr auto decay    = "body.decay";
    inline constexpr auto brightness = "body.brightness";
    inline constexpr auto modes    = "body.modes";
    inline constexpr auto preset   = "body.preset";
}

namespace layer
{
    inline constexpr auto bEnable   = "layer.b.enable";
    inline constexpr auto bInterval = "layer.b.interval";
    inline constexpr auto bDetune   = "layer.b.detune";
    inline constexpr auto bLevel    = "layer.b.level";
}

namespace lfo1
{
    inline constexpr auto wave     = "lfo1.wave";
    inline constexpr auto rate     = "lfo1.rate";
    inline constexpr auto sync     = "lfo1.sync";
    inline constexpr auto division = "lfo1.division";
    inline constexpr auto depth    = "lfo1.depth";
    inline constexpr auto dest     = "lfo1.dest";
}

namespace lfo2
{
    inline constexpr auto wave     = "lfo2.wave";
    inline constexpr auto rate     = "lfo2.rate";
    inline constexpr auto sync     = "lfo2.sync";
    inline constexpr auto division = "lfo2.division";
    inline constexpr auto depth    = "lfo2.depth";
    inline constexpr auto dest     = "lfo2.dest";
}

namespace env
{
    inline constexpr auto attack  = "env.attack";
    inline constexpr auto decay   = "env.decay";
    inline constexpr auto sustain = "env.sustain";
    inline constexpr auto release = "env.release";
    inline constexpr auto depth   = "env.depth";
    inline constexpr auto dest    = "env.dest";
}

namespace chaos
{
    inline constexpr auto amount   = "chaos.amount";
    inline constexpr auto rate     = "chaos.rate";
    inline constexpr auto sync     = "chaos.sync";
    inline constexpr auto division = "chaos.division";
    inline constexpr auto bias     = "chaos.bias";
}

namespace macros
{
    inline constexpr auto material   = "macro.material";
    inline constexpr auto attack     = "macro.attack";
    inline constexpr auto decay      = "macro.decay";
    inline constexpr auto brightness = "macro.brightness";
    inline constexpr auto body       = "macro.body";
    inline constexpr auto chaos      = "macro.chaos";
    inline constexpr auto space      = "macro.space";
    inline constexpr auto drive      = "macro.drive";
}

namespace fx
{
    inline constexpr auto filterType      = "fx.filter.type";
    inline constexpr auto filterCutoff    = "fx.filter.cutoff";
    inline constexpr auto filterResonance = "fx.filter.res";
    inline constexpr auto filterMix       = "fx.filter.mix";

    inline constexpr auto satMode  = "fx.sat.mode";
    inline constexpr auto satDrive = "fx.sat.drive";
    inline constexpr auto satTone  = "fx.sat.tone";
    inline constexpr auto satMix   = "fx.sat.mix";

    inline constexpr auto delayTimeL     = "fx.delay.timel";
    inline constexpr auto delayTimeR     = "fx.delay.timer";
    inline constexpr auto delaySync      = "fx.delay.sync";
    inline constexpr auto delayDivisionL = "fx.delay.divisionl";
    inline constexpr auto delayDivisionR = "fx.delay.divisionr";
    inline constexpr auto delayFeedback  = "fx.delay.feedback";
    inline constexpr auto delayMix       = "fx.delay.mix";
    inline constexpr auto delayDamp      = "fx.delay.damp";

    inline constexpr auto chorusRate     = "fx.chorus.rate";
    inline constexpr auto chorusSync     = "fx.chorus.sync";
    inline constexpr auto chorusDivision = "fx.chorus.division";
    inline constexpr auto chorusDepth    = "fx.chorus.depth";
    inline constexpr auto chorusMix      = "fx.chorus.mix";

    inline constexpr auto phaserRate     = "fx.phaser.rate";
    inline constexpr auto phaserSync     = "fx.phaser.sync";
    inline constexpr auto phaserDivision = "fx.phaser.division";
    inline constexpr auto phaserDepth    = "fx.phaser.depth";
    inline constexpr auto phaserFeedback = "fx.phaser.feedback";
    inline constexpr auto phaserMix      = "fx.phaser.mix";

    inline constexpr auto reverbSize   = "fx.reverb.size";
    inline constexpr auto reverbDecay  = "fx.reverb.decay";
    inline constexpr auto reverbDamp   = "fx.reverb.damp";
    inline constexpr auto reverbWidth  = "fx.reverb.width";
    inline constexpr auto reverbMix    = "fx.reverb.mix";
}

} // namespace aethr::params

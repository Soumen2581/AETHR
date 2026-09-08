#pragma once

#include <algorithm>

namespace aethr::dsp
{

enum class Material
{
    string = 0,
    nylon,
    steel,
    wood,
    glass,
    metal,
    ceramic,
    rubber,
    crystal,
    hollow,
    synthetic,
    alien,
    numTypes
};

/**
    A material is a musically designed starting point, not a sample.

    These numbers are written into the underlying parameters when the user picks
    a material. They remain fully overridable afterwards.
*/
struct MaterialProfile
{
    float dampingPercent { 35.0f };
    float brightnessPercent { 60.0f };
    float decaySeconds { 1.6f };
    float releaseSeconds { 0.25f };
    float stiffnessPercent { 0.0f };
    float bodyMixPercent { 0.0f };
    float bodyDecayPercent { 40.0f };
    int   bodyPresetIndex { 0 };
    int   bodyModesIndex { 2 };     // 4 modes
    int   loopFilterIndex { 1 };    // one-pole
    int   exciterTypeIndex { 0 };   // pluck
};

[[nodiscard]] inline MaterialProfile profileFor (Material material) noexcept
{
    MaterialProfile profile;

    switch (material)
    {
        case Material::string:
            break;
        case Material::nylon:
            profile.dampingPercent = 48.0f;
            profile.brightnessPercent = 42.0f;
            profile.decaySeconds = 1.1f;
            profile.stiffnessPercent = 4.0f;
            profile.exciterTypeIndex = 0;
            break;
        case Material::steel:
            profile.dampingPercent = 18.0f;
            profile.brightnessPercent = 78.0f;
            profile.decaySeconds = 3.2f;
            profile.stiffnessPercent = 22.0f;
            profile.bodyMixPercent = 8.0f;
            profile.bodyPresetIndex = 1;
            profile.exciterTypeIndex = 5;
            break;
        case Material::wood:
            profile.dampingPercent = 62.0f;
            profile.brightnessPercent = 38.0f;
            profile.decaySeconds = 0.55f;
            profile.releaseSeconds = 0.12f;
            profile.bodyMixPercent = 35.0f;
            profile.bodyPresetIndex = 0;
            profile.exciterTypeIndex = 3;
            profile.loopFilterIndex = 2;
            break;
        case Material::glass:
            profile.dampingPercent = 12.0f;
            profile.brightnessPercent = 88.0f;
            profile.decaySeconds = 4.5f;
            profile.stiffnessPercent = 55.0f;
            profile.bodyMixPercent = 28.0f;
            profile.bodyPresetIndex = 2;
            profile.bodyModesIndex = 3;
            profile.exciterTypeIndex = 4;
            break;
        case Material::metal:
            profile.dampingPercent = 10.0f;
            profile.brightnessPercent = 82.0f;
            profile.decaySeconds = 5.0f;
            profile.stiffnessPercent = 70.0f;
            profile.bodyMixPercent = 22.0f;
            profile.bodyPresetIndex = 1;
            profile.bodyModesIndex = 4;
            profile.exciterTypeIndex = 5;
            break;
        case Material::ceramic:
            profile.dampingPercent = 28.0f;
            profile.brightnessPercent = 70.0f;
            profile.decaySeconds = 1.8f;
            profile.stiffnessPercent = 40.0f;
            profile.bodyMixPercent = 18.0f;
            profile.bodyPresetIndex = 2;
            profile.exciterTypeIndex = 3;
            break;
        case Material::rubber:
            profile.dampingPercent = 85.0f;
            profile.brightnessPercent = 22.0f;
            profile.decaySeconds = 0.18f;
            profile.releaseSeconds = 0.08f;
            profile.stiffnessPercent = 8.0f;
            profile.bodyMixPercent = 12.0f;
            profile.bodyPresetIndex = 5;
            profile.exciterTypeIndex = 3;
            profile.loopFilterIndex = 2;
            break;
        case Material::crystal:
            profile.dampingPercent = 6.0f;
            profile.brightnessPercent = 92.0f;
            profile.decaySeconds = 8.0f;
            profile.stiffnessPercent = 48.0f;
            profile.bodyMixPercent = 40.0f;
            profile.bodyPresetIndex = 6;
            profile.bodyModesIndex = 4;
            profile.exciterTypeIndex = 4;
            break;
        case Material::hollow:
            profile.dampingPercent = 40.0f;
            profile.brightnessPercent = 50.0f;
            profile.decaySeconds = 0.9f;
            profile.bodyMixPercent = 55.0f;
            profile.bodyPresetIndex = 3;
            profile.bodyModesIndex = 2;
            profile.exciterTypeIndex = 1;
            break;
        case Material::synthetic:
            profile.dampingPercent = 25.0f;
            profile.brightnessPercent = 65.0f;
            profile.decaySeconds = 2.4f;
            profile.stiffnessPercent = 15.0f;
            profile.bodyMixPercent = 10.0f;
            profile.exciterTypeIndex = 2;
            break;
        case Material::alien:
            profile.dampingPercent = 8.0f;
            profile.brightnessPercent = 95.0f;
            profile.decaySeconds = 12.0f;
            profile.stiffnessPercent = 88.0f;
            profile.bodyMixPercent = 45.0f;
            profile.bodyPresetIndex = 6;
            profile.bodyModesIndex = 4;
            profile.exciterTypeIndex = 6;
            break;
        case Material::numTypes:
        default:
            break;
    }

    return profile;
}

} // namespace aethr::dsp

#pragma once

#include <cstdint>

/**
    Physical-engine catalogue.

    Indices are part of the `engine.type` parameter contract. New engines are
    appended; an index is never reused for a different model. Every catalogue
    entry currently dispatches to a distinct core or character.
*/
namespace aethr::engine
{

enum class EngineType : std::uint8_t
{
    string = 0,
    pluck,
    bowed,
    bell,
    plate,
    membrane,
    tube,
    cavity,
    waveguide,
    modal,
    granular,
    spectral,
    hybrid,
    numTypes
};

enum class EngineFamily : std::uint8_t
{
    physical = 0,
    synthetic,
    experimental
};

struct EngineInfo
{
    EngineType type;
    EngineFamily family;
    const char* name;
    const char* code;
    bool implemented;
};

inline constexpr int numEngineTypes = static_cast<int> (EngineType::numTypes);
inline constexpr int defaultEngineIndex = static_cast<int> (EngineType::string);

inline constexpr EngineInfo engineCatalogue[numEngineTypes] {
    { EngineType::string,     EngineFamily::physical,     "String",     "STR", true },
    { EngineType::pluck,      EngineFamily::physical,     "Pluck",      "PLK", true },
    { EngineType::bowed,      EngineFamily::physical,     "Bowed",      "BWD", true },
    { EngineType::bell,       EngineFamily::physical,     "Bell",       "BEL", true },
    { EngineType::plate,      EngineFamily::physical,     "Plate",      "PLT", true },
    { EngineType::membrane,   EngineFamily::physical,     "Membrane",   "MBR", true },
    { EngineType::tube,       EngineFamily::physical,     "Tube",       "TUB", true },
    { EngineType::cavity,     EngineFamily::physical,     "Cavity",     "CVT", true },
    { EngineType::waveguide,  EngineFamily::synthetic,    "Waveguide",  "WVG", true },
    { EngineType::modal,      EngineFamily::synthetic,    "Modal",      "MDL", true },
    { EngineType::granular,   EngineFamily::experimental, "Granular",   "GRN", true },
    { EngineType::spectral,   EngineFamily::synthetic,    "Spectral",   "SPC", true },
    { EngineType::hybrid,     EngineFamily::experimental, "Hybrid",     "HYB", true }
};

[[nodiscard]] inline EngineType engineTypeFromIndex (int index) noexcept
{
    if (index < 0 || index >= numEngineTypes)
        return EngineType::string;

    return static_cast<EngineType> (index);
}

[[nodiscard]] inline const EngineInfo& infoFor (EngineType type) noexcept
{
    return engineCatalogue[static_cast<int> (type)];
}

[[nodiscard]] inline bool isEngineImplemented (EngineType) noexcept
{
    return true;
}

[[nodiscard]] inline EngineType resolvedEngineType (EngineType requested) noexcept
{
    return requested;
}

[[nodiscard]] inline bool usesModalCore (EngineType type) noexcept
{
    switch (type)
    {
        case EngineType::bell:
        case EngineType::plate:
        case EngineType::membrane:
        case EngineType::modal:
        case EngineType::cavity:
        case EngineType::spectral:
            return true;
        case EngineType::string:
        case EngineType::pluck:
        case EngineType::bowed:
        case EngineType::tube:
        case EngineType::waveguide:
        case EngineType::granular:
        case EngineType::hybrid:
        case EngineType::numTypes:
            return false;
    }

    return false;
}

[[nodiscard]] inline bool usesHybridCore (EngineType type) noexcept
{
    return type == EngineType::hybrid;
}

struct EngineControlNames
{
    const char* a;
    const char* b;
    const char* c;
    const char* d;
};

[[nodiscard]] inline EngineControlNames controlNamesFor (EngineType type) noexcept
{
    switch (type)
    {
        case EngineType::pluck:      return { "Pluck", "Position", "Tension", "Pickup" };
        case EngineType::bowed:      return { "Pressure", "Speed", "Friction", "Position" };
        case EngineType::bell:       return { "Strike", "Inharmonic", "Partial", "Bloom" };
        case EngineType::plate:      return { "Size", "Stiff", "Hit", "Spread" };
        case EngineType::membrane:   return { "Tension", "Size", "Hit", "Damp" };
        case EngineType::tube:       return { "Length", "Breath", "Open", "Turbulence" };
        case EngineType::cavity:     return { "Volume", "Opening", "Air", "Coupling" };
        case EngineType::waveguide:  return { "Scatter", "Dispersion", "Node", "Feedback" };
        case EngineType::modal:      return { "Modes", "Stretch", "Q", "Tilt" };
        case EngineType::granular:   return { "Size", "Density", "Pitch", "Spray" };
        case EngineType::spectral:   return { "Partials", "Stretch", "Freeze", "Diffuse" };
        case EngineType::hybrid:     return { "Balance", "Couple", "Cross", "Limit" };
        case EngineType::string:
        case EngineType::numTypes:
            return { "Tension", "Position", "Loss", "Pickup" };
    }

    return { "Tension", "Position", "Loss", "Pickup" };
}

} // namespace aethr::engine

# ==============================================================================
#  First-party sources.
#
#  Files are listed explicitly rather than globbed so that adding a file is a
#  reviewable change and so CMake reliably reconfigures.
# ==============================================================================

set(AETHR_HEADERS
    Core/Branding.h
    Core/AudioMath.h
    Core/RealtimeGuards.h
    Core/TempoSync.h
    DSP/PhaseDelay.h
    DSP/FractionalDelayLine.h
    DSP/LoopFilter.h
    DSP/KarplusResonator.h
    DSP/Exciter.h
    DSP/Dispersion.h
    DSP/BodyResonator.h
    DSP/PhysicalMaterial.h
    DSP/Modulation.h
    DSP/FxRack.h
    Engine/EngineSettings.h
    Engine/EngineType.h
    Engine/KarplusStringEngine.h
    Engine/ModalEngine.h
    Engine/Arpeggiator.h
    Engine/Voice.h
    Engine/VoiceEngine.h
    Parameters/ParameterIDs.h
    Parameters/ParameterLayout.h
    Presets/PresetManager.h
    UI/Theme.h
    UI/LookAndFeel.h
    UI/Controls.h
    UI/EngineBrowser.h
    UI/ArpStrip.h
    UI/Visualizers.h
    UI/PresetBrowser.h
    PluginProcessor.h
    PluginEditor.h)

set(AETHR_SOURCES
    DSP/KarplusResonator.cpp
    DSP/Exciter.cpp
    Engine/KarplusStringEngine.cpp
    Engine/Voice.cpp
    Engine/VoiceEngine.cpp
    Parameters/ParameterLayout.cpp
    PluginProcessor.cpp
    PluginEditor.cpp)

target_sources(Aethr PRIVATE ${AETHR_SOURCES} ${AETHR_HEADERS})

# Keep the aggressive warning set on our code only - see cmake/AethrWarnings.cmake.
list(TRANSFORM AETHR_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/" OUTPUT_VARIABLE AETHR_SOURCES_ABS)
aethr_apply_strict_warnings(${AETHR_SOURCES_ABS})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${AETHR_SOURCES} ${AETHR_HEADERS})

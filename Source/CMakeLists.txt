# ==============================================================================
#  First-party sources.
#
#  Files are listed explicitly rather than globbed so that adding a file is a
#  reviewable change and so CMake reliably reconfigures.
# ==============================================================================

set(STRATA_HEADERS
    Core/Branding.h
    Core/AudioMath.h
    Core/RealtimeGuards.h
    Parameters/ParameterIDs.h
    Parameters/ParameterLayout.h
    PluginProcessor.h
    PluginEditor.h)

set(STRATA_SOURCES
    Parameters/ParameterLayout.cpp
    PluginProcessor.cpp
    PluginEditor.cpp)

target_sources(Strata PRIVATE ${STRATA_SOURCES} ${STRATA_HEADERS})

# Keep the aggressive warning set on our code only - see cmake/StrataWarnings.cmake.
list(TRANSFORM STRATA_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/" OUTPUT_VARIABLE STRATA_SOURCES_ABS)
strata_apply_strict_warnings(${STRATA_SOURCES_ABS})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${STRATA_SOURCES} ${STRATA_HEADERS})

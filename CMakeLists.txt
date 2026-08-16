# ==============================================================================
#  STRATA - Physical Resonance Engine
#
#  Build requirements (verified 2026-08-16 on the development machine):
#    - CMake >= 3.25   (installed: 4.3.2)
#    - JUCE 9.0.1      (pinned tag; JUCE itself requires cmake_minimum_required 3.22)
#    - C++23           (Apple clang 21 / MSVC 19.4x / GCC 14+)
#
#  JUCE 9 requires the C language to be enabled in the consuming project, see
#  External/JUCE/CMakeLists.txt: "A C compiler is required to build targets that
#  depend on JUCE." Hence LANGUAGES C CXX below - do not remove C.
# ==============================================================================

cmake_minimum_required(VERSION 3.25)

# ------------------------------------------------------------------------------
# Branding. Everything user-visible is derived from these values so the product
# can be renamed without touching source code. See docs/ARCHITECTURE.md.
# ------------------------------------------------------------------------------
set(STRATA_PRODUCT_NAME    "STRATA"                        CACHE STRING "User-visible product name")
set(STRATA_PRODUCT_TAGLINE "Physical Resonance Engine"     CACHE STRING "Subtitle shown in the UI")
set(STRATA_COMPANY_NAME    "BrainWavez"                    CACHE STRING "Manufacturer name")
set(STRATA_COMPANY_DOMAIN  "brainwavez.audio"              CACHE STRING "Manufacturer domain (reverse-DNS bundle IDs)")
set(STRATA_BUNDLE_ID       "audio.brainwavez.strata"       CACHE STRING "macOS bundle identifier")
set(STRATA_MANUFACTURER_ID "Bwvz"                          CACHE STRING "4-character manufacturer code (must start uppercase)")
set(STRATA_PLUGIN_ID       "Strq"                          CACHE STRING "4-character plugin code (must start uppercase)")

project(STRATA
    VERSION 0.1.0
    DESCRIPTION "${STRATA_PRODUCT_NAME} - ${STRATA_PRODUCT_TAGLINE}"
    LANGUAGES C CXX)

# ------------------------------------------------------------------------------
# Global language / platform settings
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Interprocedural optimisation is enabled per-config via JUCE's LTO flag target
# rather than globally, so Debug builds stay fast to link.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    # macOS 11 (Big Sur) is the lowest release that supports Apple Silicon, so it
    # is the natural floor for a universal binary. Justified in docs/BUILD.md.
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum supported macOS version")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build configuration" FORCE)
endif()

# Always emit a compilation database for clang-tidy / editors.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------
option(STRATA_BUILD_TESTS      "Build the offline DSP/state test suite" ON)
option(STRATA_BUILD_STANDALONE "Build the standalone application"       ON)
option(STRATA_BUILD_AU         "Build the Audio Unit (macOS only)"      ON)
option(STRATA_WARNINGS_AS_ERRORS "Treat compiler warnings as errors (CI/dev)" OFF)
option(STRATA_COPY_AFTER_BUILD "Install the plugin to the user plug-in folders after building" ON)

# ------------------------------------------------------------------------------
# Dependencies
#
# JUCE is pinned to an exact tag for reproducibility - never a moving branch.
# If External/JUCE already contains a checkout, it is reused instead of being
# re-downloaded (FetchContent honours FETCHCONTENT_SOURCE_DIR_<NAME>).
# ------------------------------------------------------------------------------
include(FetchContent)

set(STRATA_JUCE_TAG "9.0.1" CACHE STRING "Exact JUCE git tag to build against")
set(STRATA_CATCH2_TAG "v3.9.1" CACHE STRING "Exact Catch2 git tag to build against")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/External/JUCE/CMakeLists.txt"
        AND NOT DEFINED FETCHCONTENT_SOURCE_DIR_JUCE)
    set(FETCHCONTENT_SOURCE_DIR_JUCE "${CMAKE_CURRENT_SOURCE_DIR}/External/JUCE"
        CACHE PATH "Local JUCE checkout reused instead of downloading")
endif()

set(JUCE_BUILD_EXTRAS   OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${STRATA_JUCE_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    SYSTEM)                     # treat JUCE headers as system headers so our
                                # -Werror never fires on framework code
FetchContent_MakeAvailable(JUCE)

# ------------------------------------------------------------------------------
# Shared compile settings
#
# Denormal/NaN handling and the stability guards in the feedback paths rely on
# strict IEEE-754 semantics, so fast-math stays off everywhere and floating point
# contraction is disabled to keep offline test renders bit-comparable across
# compilers. This is a deliberate, documented decision (docs/DSP_NOTES.md).
# ------------------------------------------------------------------------------
include(cmake/StrataWarnings.cmake)

add_library(strata_compile_options INTERFACE)

if(NOT MSVC)
    target_compile_options(strata_compile_options INTERFACE -ffp-contract=off)
else()
    target_compile_options(strata_compile_options INTERFACE
        /fp:precise /Zc:__cplusplus /Zc:preprocessor /utf-8)
endif()

# ------------------------------------------------------------------------------
# Plugin target
#
# juce_add_plugin() creates a STATIC library holding the shared code plus one
# target per format. The test suite links the shared-code target directly, so
# JUCE modules are compiled exactly once.
# ------------------------------------------------------------------------------
set(STRATA_FORMATS VST3)

if(STRATA_BUILD_STANDALONE)
    list(APPEND STRATA_FORMATS Standalone)
endif()

if(APPLE AND STRATA_BUILD_AU)
    list(APPEND STRATA_FORMATS AU)
endif()

juce_add_plugin(Strata
    PRODUCT_NAME                "${STRATA_PRODUCT_NAME}"
    COMPANY_NAME                "${STRATA_COMPANY_NAME}"
    COMPANY_WEBSITE             "https://${STRATA_COMPANY_DOMAIN}"
    BUNDLE_ID                   "${STRATA_BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE    "${STRATA_MANUFACTURER_ID}"
    PLUGIN_CODE                 "${STRATA_PLUGIN_ID}"
    DESCRIPTION                 "${STRATA_PRODUCT_TAGLINE}"
    FORMATS                     ${STRATA_FORMATS}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD     ${STRATA_COPY_AFTER_BUILD}
    MICROPHONE_PERMISSION_ENABLED FALSE
    HARDENED_RUNTIME_ENABLED    TRUE)

add_subdirectory(Source)

target_include_directories(Strata PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_compile_definitions(Strata
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        # JUCE 9 has no splash screen, so JUCE_DISPLAY_SPLASH_SCREEN is gone -
        # setting it produces a compiler warning.
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        STRATA_PRODUCT_NAME="${STRATA_PRODUCT_NAME}"
        STRATA_PRODUCT_TAGLINE="${STRATA_PRODUCT_TAGLINE}"
        STRATA_COMPANY_NAME="${STRATA_COMPANY_NAME}"
        STRATA_VERSION_STRING="${PROJECT_VERSION}")

target_link_libraries(Strata
    PUBLIC
        strata_compile_options
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:juce::juce_recommended_lto_flags>)

# ------------------------------------------------------------------------------
# macOS: make freshly built bundles loadable.
#
# Build output inherits com.apple.quarantine from the process that launched the
# build. dyld then refuses to load the bundle ("library load disallowed by system
# policy") even though the binary and its ad-hoc signature are valid. This step
# clears the attributes and re-signs, for the build artefact and for the copy that
# JUCE installs into the user plug-in folders.
#
# It is registered after juce_add_plugin() so it runs after JUCE's own copy step.
# ------------------------------------------------------------------------------
if(APPLE)
    set(STRATA_SANITISE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/Tools/sanitise-macos-bundle.sh")

    foreach(format IN LISTS STRATA_FORMATS)
        set(format_target "Strata_${format}")

        if(NOT TARGET ${format_target})
            continue()
        endif()

        # The user-plug-in-folder copy, when there is one for this format.
        set(installed_copy "")

        if(format STREQUAL "VST3")
            set(installed_copy "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/${STRATA_PRODUCT_NAME}.vst3")
        elseif(format STREQUAL "AU")
            set(installed_copy "$ENV{HOME}/Library/Audio/Plug-Ins/Components/${STRATA_PRODUCT_NAME}.component")
        endif()

        add_custom_command(TARGET ${format_target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E env bash "${STRATA_SANITISE_SCRIPT}"
                    "$<TARGET_BUNDLE_DIR:${format_target}>" ${installed_copy}
            VERBATIM
            COMMENT "Clearing quarantine and re-signing ${format_target}")
    endforeach()
endif()

# ------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------
if(STRATA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

# ------------------------------------------------------------------------------
# Configuration summary
# ------------------------------------------------------------------------------
message(STATUS "")
message(STATUS "${STRATA_PRODUCT_NAME} ${PROJECT_VERSION} - ${STRATA_PRODUCT_TAGLINE}")
message(STATUS "  JUCE tag ........ ${STRATA_JUCE_TAG}")
message(STATUS "  C++ standard .... C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Formats ......... ${STRATA_FORMATS}")
message(STATUS "  Tests ........... ${STRATA_BUILD_TESTS} (Catch2 ${STRATA_CATCH2_TAG})")
message(STATUS "  Warnings=errors . ${STRATA_WARNINGS_AS_ERRORS}")
message(STATUS "")

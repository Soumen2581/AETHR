# ==============================================================================
#  Offline test suite.
#
#  The tests link the shared-code target created by juce_add_plugin(), so the
#  JUCE modules are compiled exactly once for the whole project and the code
#  under test is bit-identical to what ships inside the plugin binaries.
# ==============================================================================

include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        ${AETHR_CATCH2_TAG}
    GIT_SHALLOW    TRUE
    SYSTEM)
FetchContent_MakeAvailable(Catch2)

set(AETHR_TEST_SOURCES
    TestMain.cpp
    PitchEstimator.cpp
    CoreMathTests.cpp
    GuardTests.cpp
    ParameterTests.cpp
    ProcessorTests.cpp
    PhaseDelayTests.cpp
    ResonatorTests.cpp
    ExciterTests.cpp
    PitchTests.cpp
    EngineTests.cpp)

add_executable(AethrTests ${AETHR_TEST_SOURCES})

list(TRANSFORM AETHR_TEST_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/" OUTPUT_VARIABLE AETHR_TEST_SOURCES_ABS)
aethr_apply_strict_warnings(${AETHR_TEST_SOURCES_ABS})

# The safety guards promise to substitute *exactly* zero, and the gain floor
# promises *exact* silence. Asserting that requires exact comparisons, so the
# warning against them is switched off for test code only.
aethr_relax_source_warnings(-Wno-float-equal ${AETHR_TEST_SOURCES_ABS})

target_link_libraries(AethrTests
    PRIVATE
        Aethr
        Catch2::Catch2WithMain)

# JUCE_STANDALONE_APPLICATION is already defined by the shared-code target (as
# JucePlugin_Build_Standalone) and propagates here, so it must not be set again.
target_compile_definitions(AethrTests PRIVATE JUCE_UNIT_TESTS=1)

list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
include(Catch)
catch_discover_tests(AethrTests)

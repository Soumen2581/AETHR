# AETHR packaging — shared helpers and entry points.
#
# Authoritative version: project(AETHR VERSION …) → PROJECT_VERSION
# Artefact root (JUCE): ${CMAKE_BINARY_DIR}/Aethr_artefacts/$<CONFIG>/

set(AETHR_DIST_ROOT "${CMAKE_SOURCE_DIR}/dist" CACHE PATH "Installer output root")
set(AETHR_PACKAGING_DIR "${CMAKE_SOURCE_DIR}/packaging")

function(aethr_artefacts_config_dir out_var)
    # Multi-config generators need $<CONFIG>; single-config uses CMAKE_BUILD_TYPE.
    if(CMAKE_CONFIGURATION_TYPES)
        set(${out_var} "${CMAKE_BINARY_DIR}/Aethr_artefacts/$<CONFIG>" PARENT_SCOPE)
    else()
        set(${out_var} "${CMAKE_BINARY_DIR}/Aethr_artefacts/${CMAKE_BUILD_TYPE}" PARENT_SCOPE)
    endif()
endfunction()

aethr_artefacts_config_dir(AETHR_ARTEFACTS_DIR)

include("${CMAKE_CURRENT_LIST_DIR}/MacOSPackaging.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/WindowsPackaging.cmake")

message(STATUS "  Packaging ....... dist/ → ${AETHR_DIST_ROOT}")

# ==============================================================================
#  Strict warning flags for first-party code.
#
#  JUCE attaches its module .cpp files as INTERFACE sources of the module
#  targets (see JUCEModuleSupport.cmake, _juce_add_interface_library), which
#  means they are compiled *into* the consuming target using that target's
#  compile options. Applying an aggressive warning set at target level would
#  therefore flood the build with warnings from framework code we do not own.
#
#  So: JUCE's own curated set (juce::juce_recommended_warning_flags) is linked at
#  target level, and the aggressive set below is applied per source file to files
#  we actually maintain.
# ==============================================================================

if(MSVC)
    set(STRATA_STRICT_WARNINGS
        /W4
        /permissive-)

    set(STRATA_WARNINGS_AS_ERRORS_FLAG /WX)
else()
    set(STRATA_STRICT_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        -Wcast-align
        -Wunused
        -Wsign-conversion
        -Wdouble-promotion
        -Wfloat-equal
        -Wextra-semi
        -Wimplicit-fallthrough)

    set(STRATA_WARNINGS_AS_ERRORS_FLAG -Werror)
endif()

# Applies the strict warning set (and -Werror when requested) to the given
# source files only.
function(strata_apply_strict_warnings)
    set(flags ${STRATA_STRICT_WARNINGS})

    if(STRATA_WARNINGS_AS_ERRORS)
        list(APPEND flags ${STRATA_WARNINGS_AS_ERRORS_FLAG})
    endif()

    foreach(source_file IN LISTS ARGN)
        get_source_file_property(existing "${source_file}" COMPILE_OPTIONS)

        if(existing STREQUAL "NOTFOUND")
            set(existing "")
        endif()

        set_source_files_properties("${source_file}" PROPERTIES
            COMPILE_OPTIONS "${existing};${flags}")
    endforeach()
endfunction()

# Appends extra flags after the strict set for specific files. Used to switch off a
# warning where violating it is the deliberate point of the code - for example the
# test suite, which must compare floating-point results exactly to prove that a
# guard substitutes exactly zero rather than something merely small.
function(strata_relax_source_warnings flag)
    foreach(source_file IN LISTS ARGN)
        get_source_file_property(existing "${source_file}" COMPILE_OPTIONS)

        if(existing STREQUAL "NOTFOUND")
            set(existing "")
        endif()

        set_source_files_properties("${source_file}" PROPERTIES
            COMPILE_OPTIONS "${existing};${flag}")
    endforeach()
endfunction()

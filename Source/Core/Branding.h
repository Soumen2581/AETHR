#pragma once

/**
    Single source of truth for user-visible product identity.

    The values are injected by CMake (see the top-level CMakeLists.txt) so the
    product can be renamed by editing the build script alone. Nothing in the DSP
    or parameter layer may depend on these strings.
*/
namespace strata::branding
{

#ifndef STRATA_PRODUCT_NAME
    #define STRATA_PRODUCT_NAME "STRATA"
#endif

#ifndef STRATA_PRODUCT_TAGLINE
    #define STRATA_PRODUCT_TAGLINE "Physical Resonance Engine"
#endif

#ifndef STRATA_COMPANY_NAME
    #define STRATA_COMPANY_NAME "BrainWavez"
#endif

#ifndef STRATA_VERSION_STRING
    #define STRATA_VERSION_STRING "0.0.0"
#endif

inline constexpr const char* productName = STRATA_PRODUCT_NAME;
inline constexpr const char* tagline     = STRATA_PRODUCT_TAGLINE;
inline constexpr const char* companyName = STRATA_COMPANY_NAME;
inline constexpr const char* version     = STRATA_VERSION_STRING;

} // namespace strata::branding

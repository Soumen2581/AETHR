#pragma once

/**
    Single source of truth for user-visible product identity.

    The values are injected by CMake (see the top-level CMakeLists.txt) so the
    product can be renamed by editing the build script alone. Nothing in the DSP
    or parameter layer may depend on these strings.
*/
namespace aethr::branding
{

#ifndef AETHR_PRODUCT_NAME
    #define AETHR_PRODUCT_NAME "AETHR"
#endif

#ifndef AETHR_PRODUCT_TAGLINE
    #define AETHR_PRODUCT_TAGLINE "Physical Resonance Engine"
#endif

#ifndef AETHR_COMPANY_NAME
    #define AETHR_COMPANY_NAME "ixmuk"
#endif

#ifndef AETHR_VERSION_STRING
    #define AETHR_VERSION_STRING "0.0.0"
#endif

inline constexpr const char* productName = AETHR_PRODUCT_NAME;
inline constexpr const char* tagline     = AETHR_PRODUCT_TAGLINE;
inline constexpr const char* companyName = AETHR_COMPANY_NAME;
inline constexpr const char* version     = AETHR_VERSION_STRING;

} // namespace aethr::branding

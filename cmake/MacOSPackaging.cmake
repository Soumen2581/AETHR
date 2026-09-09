# macOS: stage → component package → DMG (optional sign / notarize via env).

if(NOT APPLE)
    return()
endif()

set(AETHR_PACKAGE_MACOS_SCRIPT "${CMAKE_SOURCE_DIR}/Tools/package_macos.sh")

add_custom_target(package-macos
    COMMAND "${CMAKE_COMMAND}" -E env
            "AETHR_VERSION=${PROJECT_VERSION}"
            "AETHR_PRODUCT_NAME=${AETHR_PRODUCT_NAME}"
            "AETHR_COMPANY_NAME=${AETHR_COMPANY_NAME}"
            "AETHR_BUNDLE_ID=${AETHR_BUNDLE_ID}"
            "AETHR_ARTEFACTS_DIR=${AETHR_ARTEFACTS_DIR}"
            "AETHR_DIST_DIR=${AETHR_DIST_ROOT}/macos"
            "AETHR_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            bash "${AETHR_PACKAGE_MACOS_SCRIPT}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building macOS DMG installer for ${AETHR_PRODUCT_NAME} ${PROJECT_VERSION}"
    VERBATIM)

# Convenience: package after a full build of all formats.
add_custom_target(package
    DEPENDS package-macos
    COMMENT "Platform package (macOS)")

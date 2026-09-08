# Windows: Inno Setup → AETHR-x.y.z-Windows.exe

if(NOT WIN32)
    return()
endif()

set(AETHR_PACKAGE_WINDOWS_SCRIPT "${CMAKE_SOURCE_DIR}/Tools/package_windows.ps1")

add_custom_target(package-windows
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${AETHR_PACKAGE_WINDOWS_SCRIPT}"
            -Version "${PROJECT_VERSION}"
            -ProductName "${AETHR_PRODUCT_NAME}"
            -CompanyName "${AETHR_COMPANY_NAME}"
            -ArtefactsDir "${AETHR_ARTEFACTS_DIR}"
            -DistDir "${AETHR_DIST_ROOT}/windows"
            -SourceDir "${CMAKE_SOURCE_DIR}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Building Windows EXE installer for ${AETHR_PRODUCT_NAME} ${PROJECT_VERSION}"
    VERBATIM)

add_custom_target(package
    DEPENDS package-windows
    COMMENT "Platform package (Windows)")

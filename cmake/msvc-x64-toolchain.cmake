set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# Use vswhere to find the latest Visual Studio installation
execute_process(
    COMMAND "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer/vswhere.exe"
        -latest -property installationPath
    OUTPUT_VARIABLE VS_INSTALL_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT VS_INSTALL_PATH)
    message(FATAL_ERROR "Could not locate Visual Studio via vswhere.exe")
endif()

# Normalize to forward slashes
string(REPLACE "\\" "/" VS_INSTALL_PATH "${VS_INSTALL_PATH}")

# Find the MSVC toolset directory (latest version)
file(GLOB MSVC_VERSIONS "${VS_INSTALL_PATH}/VC/Tools/MSVC/*")
list(SORT MSVC_VERSIONS ORDER DESCENDING)
list(GET MSVC_VERSIONS 0 MSVC_TOOLSET_DIR)

# Find the Windows SDK
set(WIN10_SDK_ROOT "$ENV{ProgramFiles\(x86\)}/Windows Kits/10")
string(REPLACE "\\" "/" WIN10_SDK_ROOT "${WIN10_SDK_ROOT}")
file(GLOB WIN10_SDK_VERSIONS "${WIN10_SDK_ROOT}/bin/10.*")
list(SORT WIN10_SDK_VERSIONS ORDER DESCENDING)
list(GET WIN10_SDK_VERSIONS 0 WIN10_SDK_BIN)
get_filename_component(WIN10_SDK_VERSION "${WIN10_SDK_BIN}" NAME)

# Compilers
set(CMAKE_C_COMPILER "${MSVC_TOOLSET_DIR}/bin/Hostx64/x64/cl.exe")
set(CMAKE_CXX_COMPILER "${MSVC_TOOLSET_DIR}/bin/Hostx64/x64/cl.exe")
set(CMAKE_LINKER "${MSVC_TOOLSET_DIR}/bin/Hostx64/x64/link.exe")
set(CMAKE_RC_COMPILER "${WIN10_SDK_ROOT}/bin/${WIN10_SDK_VERSION}/x64/rc.exe")
set(CMAKE_MT "${WIN10_SDK_ROOT}/bin/${WIN10_SDK_VERSION}/x64/mt.exe")

# Include paths
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
    "${MSVC_TOOLSET_DIR}/include"
    "${WIN10_SDK_ROOT}/Include/${WIN10_SDK_VERSION}/ucrt"
    "${WIN10_SDK_ROOT}/Include/${WIN10_SDK_VERSION}/shared"
    "${WIN10_SDK_ROOT}/Include/${WIN10_SDK_VERSION}/um"
    "${WIN10_SDK_ROOT}/Include/${WIN10_SDK_VERSION}/winrt"
)

# rc.exe does not inherit the C/C++ standard include directories, so pass the
# Windows SDK include roots explicitly to resource compilation.
set(_WINDOWS_RC_INCLUDE_FLAGS
    "/I\"${WIN10_SDK_ROOT}/Include/${WIN10_SDK_VERSION}/ucrt\""
    "/I\"${WIN10_SDK_ROOT}/Include/${WIN10_SDK_VERSION}/shared\""
    "/I\"${WIN10_SDK_ROOT}/Include/${WIN10_SDK_VERSION}/um\""
    "/I\"${WIN10_SDK_ROOT}/Include/${WIN10_SDK_VERSION}/winrt\""
)
list(JOIN _WINDOWS_RC_INCLUDE_FLAGS " " _WINDOWS_RC_INCLUDE_FLAGS)
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} ${_WINDOWS_RC_INCLUDE_FLAGS}")

# Library paths
link_directories(
    "${MSVC_TOOLSET_DIR}/lib/x64"
    "${WIN10_SDK_ROOT}/Lib/${WIN10_SDK_VERSION}/ucrt/x64"
    "${WIN10_SDK_ROOT}/Lib/${WIN10_SDK_VERSION}/um/x64"
)

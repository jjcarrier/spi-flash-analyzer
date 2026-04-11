cmake_minimum_required(VERSION 3.10)

project(SpiFlashAnalyzer VERSION 1.0.0 LANGUAGES CXX RC)

# Set C++ standard (matches vc2026 project: stdcpp17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories for different build types
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include AnalyzerSDK CMake config
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/sdk")
include(AnalyzerSDKConfig)

# Source files
file(GLOB SOURCE_FILES source/*.cpp)

if(WIN32)
    list(APPEND SOURCE_FILES vc2026/Resource.rc)
endif()

# Create shared library
add_library(SpiFlashAnalyzer SHARED ${SOURCE_FILES})

# Link against Saleae::AnalyzerSDK
target_link_libraries(SpiFlashAnalyzer PRIVATE Saleae::AnalyzerSDK)

# Preprocessor definitions (matches vc2026 x64 Debug/Release)
target_compile_definitions(SpiFlashAnalyzer PRIVATE
    WIN32
    _WINDOWS
    _USRDLL
    SPI_FLASH_EXPORTS
    LOGIC2
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# Set compile options for Debug and Release, compiler-specific
if(MSVC)
    # Replace default /EHsc with /EHa (async exception handling, matches vc2026)
    string(REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    target_compile_options(SpiFlashAnalyzer PRIVATE
        /W3
        /wd4251     # Disable warning 4251 (matches vc2026)
        $<$<CONFIG:Debug>:/Zi /Od>
        $<$<CONFIG:Release>:/O2 /Oi /GL>
    )
    target_link_options(SpiFlashAnalyzer PRIVATE
        $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
    )
    target_link_libraries(SpiFlashAnalyzer PRIVATE User32.lib)
else()
    target_compile_options(SpiFlashAnalyzer PRIVATE
        $<$<CONFIG:Debug>:-O0 -g -w>
        $<$<CONFIG:Release>:-O3 -w>
    )
endif()

set_target_properties(SpiFlashAnalyzer PROPERTIES
    OUTPUT_NAME "SpiFlashAnalyzer"
)

# Install rules (optional)
install(TARGETS SpiFlashAnalyzer
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

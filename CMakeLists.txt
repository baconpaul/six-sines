cmake_minimum_required(VERSION 3.28)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# I don't want to deal with filesystem from two places right now
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING "Build for 10.15")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

## This could be a function in plugin infra
string(TIMESTAMP DAY_OF_YEAR "%j")
string(TIMESTAMP YEAR "%Y")
math(EXPR PART0 "${YEAR}-2023 + 2")
math(EXPR PART1 "${DAY_OF_YEAR}/2")
message(STATUS "Using date-driven version starting with '0'. 0.${PART0}.${PART1}.0")
## end function in plugin infra

project(six-sines VERSION 0.${PART0}.${PART1}.0 LANGUAGES C CXX)
set(PRODUCT_NAME "Six Sines")


option(USE_SANITIZER "Build and link with ASAN" FALSE)
option(COPY_AFTER_BUILD "Will copy after build" TRUE)
include(cmake/compile-options.cmake)

## THis can be way better, and go to plugin infra
file(READ ${CMAKE_SOURCE_DIR}/VERSION VERSION_FILE)
string(STRIP ${VERSION_FILE} VERSION_FILE)
message(STATUS "Version is ${VERSION_FILE}")

find_package(Git)

if (Git_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE BUILD_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else ()
    set(BUILD_HASH "unknown")
endif ()

message(STATUS "Compiling with git hash ${BUILD_HASH}")
## End of way better in plugin infra bit

include(cmake/CmakeRC.cmake)
file(GLOB_RECURSE PATCHES  "resources/factory_patches/*.sxsnp")
cmrc_add_resource_library(${PROJECT_NAME}-patches NAMESPACE sixsines_patches ${PATCHES})

set(JUCE_PATH "${CMAKE_SOURCE_DIR}/libs/JUCE")
add_subdirectory(libs)

add_library(${PROJECT_NAME}-impl STATIC
        src/clap/six-sines-clap.cpp
        src/clap/six-sines-clap-entry-impl.cpp

        src/ui/six-sines-editor.cpp

        src/ui/main-panel.cpp
        src/ui/main-sub-panel.cpp
        src/ui/matrix-panel.cpp
        src/ui/matrix-sub-panel.cpp
        src/ui/self-sub-panel.cpp
        src/ui/mixer-panel.cpp
        src/ui/mixer-sub-panel.cpp
        src/ui/source-panel.cpp
        src/ui/source-sub-panel.cpp
        src/ui/preset-manager.cpp
        src/ui/macro-panel.cpp
        src/ui/finetune-sub-panel.cpp
        src/ui/mainpan-sub-panel.cpp
        src/ui/playmode-sub-panel.cpp

        src/dsp/sintable.cpp

        src/synth/synth.cpp
        src/synth/voice.cpp
        src/synth/patch.cpp
        src/synth/mod_matrix.cpp

)
target_include_directories(${PROJECT_NAME}-impl PUBLIC src)
target_compile_definitions(${PROJECT_NAME}-impl PRIVATE
        PRODUCT_NAME="${PRODUCT_NAME}"
        BUILD_HASH="${BUILD_HASH}"
        DISPLAY_VERSION="${VERSION_FILE}"
)
target_link_libraries(${PROJECT_NAME}-impl PUBLIC
        clap
)
target_link_libraries(${PROJECT_NAME}-impl PRIVATE
        simde
        clap-helpers clap-wrapper-extensions
        mts-esp-client
        fmt-header-only
        sst-basic-blocks sst-voicemanager sst-jucegui sst-cpputils
        sst-plugininfra
        sst-plugininfra::filesystem
        sst-plugininfra::tinyxml
        sst-plugininfra::strnatcmp
        sst-plugininfra::patchbase
        sst::clap_juce_shim sst::clap_juce_shim_headers
        ${PROJECT_NAME}-patches
        samplerate
)

make_clapfirst_plugins(
        TARGET_NAME ${PROJECT_NAME}
        IMPL_TARGET ${PROJECT_NAME}-impl

        OUTPUT_NAME "${PRODUCT_NAME}"

        ENTRY_SOURCE src/clap/six-sines-clap-entry.cpp

        BUNDLE_IDENTIFER "org.baconpaul.six-sines"
        BUNDLE_VERSION ${PROJECT_VERSION}

        COPY_AFTER_BUILD ${COPY_AFTER_BUILD}

        PLUGIN_FORMATS CLAP VST3 AUV2

        # You can add a target-per-standalone you want. Syntax here is
        #   target-postfix output-name clap-id
        # This allows you to make multiple standalones from a multi-plugin clap
        STANDALONE_CONFIGURATIONS
            standalone "${PRODUCT_NAME}" "org.baconpaul.six-sines"

        STANDALONE_MACOS_ICON "${CMAKE_SOURCE_DIR}/resources/mac_installer/Icon.icns"
)





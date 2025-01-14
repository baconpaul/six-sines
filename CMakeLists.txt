cmake_minimum_required(VERSION 3.28)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# I don't want to deal with filesystem from two places right now
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING "Build for 10.15")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

string(TIMESTAMP DAY_OF_YEAR "%j")
string(TIMESTAMP YEAR "%Y")
math(EXPR PART0 "${YEAR}-2023 + 2")
math(EXPR PART1 "${DAY_OF_YEAR}/2")
message(STATUS "Using date-driven version starting with '0'. 0.${PART0}.${PART1}.0")

project(six-sines VERSION 0.${PART0}.${PART1}.0 LANGUAGES C CXX)

set(PRODUCT_NAME "Six Sines")

# use asan as an option (currently mac only)
option(USE_SANITIZER "Build and link with ASAN" FALSE)
option(COPY_AFTER_BUILD "Will copy after build" TRUE)

include(cmake/compile-options.cmake)

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
        sst-plugininfra sst-plugininfra::filesystem sst-plugininfra::tinyxml sst-plugininfra::strnatcmp
        sst::clap_juce_shim sst::clap_juce_shim_headers
        ${PROJECT_NAME}-patches
)

# Hmm - why do I need this?
add_compile_definitions(CLAP_WRAPPER_LOGLEVEL=0)

set(CLAP_TARGET ${PROJECT_NAME}_clap)
add_library(${CLAP_TARGET} MODULE
        src/clap/six-sines-clap-entry.cpp
)
target_link_libraries(${CLAP_TARGET} ${PROJECT_NAME}-impl)
target_library_make_clap(TARGET ${CLAP_TARGET}
        CLAP_NAME "${PRODUCT_NAME}"
        CLAP_MACOS_BUNDLE_IDENTIFIER org.baconpaul.six-sines.clap
        CLAP_MACOS_BUNDLE_VERSION ${PROJECT_VERSION}
        COPY_AFTER_BUILD TRUE
)
set_target_properties(${CLAP_TARGET} PROPERTIES BUNDLE TRUE MACOSX_BUNDLE TRUE)

set(VST3_TARGET ${PROJECT_NAME}_vst3)
add_library(${VST3_TARGET} MODULE)
target_sources(${VST3_TARGET} PRIVATE src/clap/six-sines-clap-entry.cpp)
target_link_libraries(${VST3_TARGET} PRIVATE ${PROJECT_NAME}-impl)
target_add_vst3_wrapper(TARGET ${VST3_TARGET}
        OUTPUT_NAME "${PRODUCT_NAME}"
        SUPPORTS_ALL_NOTE_EXPRESSIONS TRUE
)
if (WIN32)
    # Question - do we want this default in the helpers
    set_target_properties(${VST3_TARGET}
            PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY VST3
    )
endif()
if (${COPY_AFTER_BUILD})
    target_vst3_copy_after_build(TARGET ${VST3_TARGET})
endif()

if (APPLE)
    set(AUV2_TARGET ${PROJECT_NAME}_auv2)
    add_library(${AUV2_TARGET} MODULE)
    target_sources(${AUV2_TARGET} PRIVATE src/clap/six-sines-clap-entry.cpp)
    target_link_libraries(${AUV2_TARGET} PRIVATE ${PROJECT_NAME}-impl)
    message(STATUS "Adding AUv2 with ${PROJECT_VERSION}")
    target_add_auv2_wrapper(
            TARGET ${AUV2_TARGET}
            OUTPUT_NAME "${PRODUCT_NAME}"
            BUNDLE_IDENTIFIER "org.baconpaul.six-sines.auv2"
            BUNDLE_VERSION ${PROJECT_VERSION}

            CLAP_TARGET_FOR_CONFIG ${PROJECT_NAME}_clap
    )

    if (${COPY_AFTER_BUILD})
        message(STATUS "ifm: will install auv2")
        target_auv2_copy_after_build(TARGET ${AUV2_TARGET})
    endif()
endif()


set(sat ${PROJECT_NAME}_standalone)
add_executable(${sat})
target_sources(${sat} PRIVATE src/clap/six-sines-clap-entry.cpp)
target_link_libraries(${sat} PRIVATE ${PROJECT_NAME}-impl)
target_add_standalone_wrapper(TARGET ${sat}
        OUTPUT_NAME "${PRODUCT_NAME}"
        STATICALLY_LINKED_CLAP_ENTRY True
        MACOS_ICON "${CMAKE_SOURCE_DIR}/resources/mac_installer/Icon.icns"
        PLUGIN_ID "org.baconpaul.six-sines")

add_custom_target(${PROJECT_NAME}_all)
add_dependencies(${PROJECT_NAME}_all
        ${PROJECT_NAME}_clap
        ${PROJECT_NAME}_standalone
        ${PROJECT_NAME}_vst3
)
if (APPLE)
    add_dependencies(${PROJECT_NAME}_all ${PROJECT_NAME}_auv2)
endif()


cmake_minimum_required(VERSION 3.28)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# I don't want to deal with filesystem from two places right now
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING "Build for 10.1")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

string(TIMESTAMP DAY_OF_YEAR "%j")
string(TIMESTAMP YEAR "%Y")
math(EXPR PART0 "${YEAR}-2023 + 2")
math(EXPR PART1 "${DAY_OF_YEAR}/2")
message(STATUS "Using date-driven version while in alpha. 0.${PART0}.${PART1}.0")

project(integer-fm VERSION 0.${PART0}.${PART1}.0 LANGUAGES C CXX)

if (APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
    set(CMAKE_OBJC_VISIBILITY_PRESET hidden)
    set(CMAKE_OBJCXX_VISIBILITY_PRESET hidden)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Never want shared if not specified")
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# use asan as an option (currently mac only)
option(USE_SANITIZER "Build and link with ASAN" FALSE)
option(COPY_AFTER_BUILD "Will copy after build" TRUE)

# Compiler specific choices
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(
            $<$<BOOL:${USE_SANITIZER}>:-fsanitize=address>
            $<$<BOOL:${USE_SANITIZER}>:-fsanitize=undefined>

            $<$<OR:$<COMPILE_LANGUAGE:CXX>,$<COMPILE_LANGUAGE:OBJC>,$<COMPILE_LANGUAGE:OBJCXX>>:-fno-char8_t>
    )

    add_link_options(
            $<$<BOOL:${USE_SANITIZER}>:-fsanitize=address>
            $<$<BOOL:${USE_SANITIZER}>:-fsanitize=undefined>
    )
endif()

if (MSVC)
    add_compile_options(
            # Set source and executable charsets to UTF-8
            $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
            # Do *not* use the new, breaking char8_t UTF-8 bits in C++20.
            $<$<COMPILE_LANGUAGE:CXX>:/Zc:char8_t->
            # make msvc define __cplulsplus properly
            $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
    )
endif()


add_subdirectory(libs/clap-libs/clap EXCLUDE_FROM_ALL)
add_subdirectory(libs/clap-libs/clap-helpers EXCLUDE_FROM_ALL)
add_subdirectory(libs/fmt EXCLUDE_FROM_ALL)

set(JUCE_PATH "${CMAKE_SOURCE_DIR}/libs/JUCE")
add_subdirectory(libs/sst/sst-clap-helpers)
add_clap_juce_shim(JUCE_PATH "${CMAKE_SOURCE_DIR}/libs/JUCE")

add_library(simde INTERFACE)
target_include_directories(simde INTERFACE libs/simde)
add_subdirectory(libs/sst/sst-basic-blocks)
add_subdirectory(libs/sst/sst-jucegui)
add_subdirectory(libs/sst/sst-voicemanager)

add_library(mts-esp-client STATIC libs/MTS-ESP/Client/libMTSClient.cpp)
target_include_directories(mts-esp-client PUBLIC libs/MTS-ESP/Client)

set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "Get em")
set(CLAP_WRAPPER_DONT_ADD_TARGETS TRUE CACHE BOOL "I'll targetize")
set(CLAP_WRAPPER_BUILD_AUV2 TRUE CACHE BOOL "It's only logical")
if (APPLE)
    # BaconPaul has jack kinda installed
    set(RTAUDIO_API_JACK FALSE CACHE BOOL "Not on apple")
endif()

add_subdirectory(libs/clap-libs/clap-wrapper)

add_executable(ifm-main src/main.cpp)
target_link_libraries(ifm-main PRIVATE sst-basic-blocks)
target_include_directories(ifm-main PRIVATE src)

add_library(ifm-impl STATIC
        src/lib.cpp
        src/clap/ifm-clap.cpp
        src/clap/ifm-clap-entry-impl.cpp
)
target_include_directories(ifm-impl PUBLIC src)
target_link_libraries(ifm-impl PUBLIC clap clap-helpers)


set(CLAP_TARGET ${PROJECT_NAME}_clap)
add_library(${CLAP_TARGET} MODULE
        src/clap/ifm-clap-entry.cpp
)
target_link_libraries(${CLAP_TARGET} ifm-impl)
target_library_make_clap(TARGET ${CLAP_TARGET}
        CLAP_NAME "IFM"
        CLAP_MACOS_BUNDLE_IDENTIFIER org.baconpaul.ifm.clap
        CLAP_MACOS_BUNDLE_VERSION ${PROJECT_VERSION}
        COPY_AFTER_BUILD TRUE
)
set_target_properties(${CLAP_TARGET} PROPERTIES BUNDLE TRUE MACOSX_BUNDLE TRUE)

set(VST3_TARGET ${PROJECT_NAME}_vst3)
add_library(${VST3_TARGET} MODULE)
target_sources(${VST3_TARGET} PRIVATE src/clap/ifm-clap-entry.cpp)
target_link_libraries(${VST3_TARGET} PRIVATE ifm-impl)
target_add_vst3_wrapper(TARGET ${VST3_TARGET}
        OUTPUT_NAME "IFM"
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
    target_sources(${AUV2_TARGET} PRIVATE src/clap/ifm-clap-entry.cpp)
    target_link_libraries(${AUV2_TARGET} PRIVATE ifm-impl)
    message(STATUS "Adding AUv2 with ${PROJECT_VERSION}")
    target_add_auv2_wrapper(
            TARGET ${AUV2_TARGET}
            OUTPUT_NAME "IFM"
            BUNDLE_IDENTIFIER "org.baconpaul.ifm.auv2"
            BUNDLE_VERSION ${PROJECT_VERSION}

            CLAP_TARGET_FOR_CONFIG ${PROJECT_NAME}_clap
    )

    if (${COPY_AFTER_BUILD})
        message(STATUS "ifm: will install auv2")
        target_auv2_copy_after_build(TARGET ${AUV2_TARGET})
    endif()
endif()


set(sat ${PROJECT_NAME}_Standalone)
add_executable(${sat})
target_sources(${sat} PRIVATE src/clap/ifm-clap-entry.cpp)
target_link_libraries(${sat} PRIVATE ifm-impl)
target_add_standalone_wrapper(TARGET ${sat}
        OUTPUT_NAME "IFM"
        STATICALLY_LINKED_CLAP_ENTRY True
        PLUGIN_ID "org.baconpaul.ifm-synth")




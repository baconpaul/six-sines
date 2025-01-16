
add_subdirectory(clap-libs/clap EXCLUDE_FROM_ALL)
add_subdirectory(clap-libs/clap-helpers EXCLUDE_FROM_ALL)
add_subdirectory(fmt EXCLUDE_FROM_ALL)

add_subdirectory(sst/sst-clap-helpers)
add_clap_juce_shim(JUCE_PATH "${JUCE_PATH}")

add_library(simde INTERFACE)
target_include_directories(simde INTERFACE simde)
add_subdirectory(sst/sst-basic-blocks)
add_subdirectory(sst/sst-jucegui)
add_subdirectory(sst/sst-voicemanager)
add_subdirectory(sst/sst-cpputils)

set(SST_PLUGININFRA_PROVIDE_TINYXML ON CACHE BOOL "no xml")
add_subdirectory(sst/sst-plugininfra)

add_library(mts-esp-client STATIC MTS-ESP/Client/libMTSClient.cpp)
target_include_directories(mts-esp-client PUBLIC MTS-ESP/Client)

set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE CACHE BOOL "Get em")
set(CLAP_WRAPPER_DONT_ADD_TARGETS TRUE CACHE BOOL "I'll targetize")
set(CLAP_WRAPPER_BUILD_AUV2 TRUE CACHE BOOL "It's only logical")
if (APPLE)
    # BaconPaul has jack kinda installed
    set(RTAUDIO_API_JACK FALSE CACHE BOOL "Not on apple")
endif()

add_subdirectory(clap-libs/clap-wrapper)

add_subdirectory(libsamplerate)
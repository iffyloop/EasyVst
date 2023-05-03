
CPMAddPackage(
    NAME VSTModuleArchitectureSDK
    VERSION  3.7.7
    URL "https://download.steinberg.net/sdk_downloads/vst-sdk_3.7.7_build-19_2022-12-12.zip"
)

add_library(VST_SDK

    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/public.sdk/source/vst/hosting/connectionproxy.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/public.sdk/source/vst/hosting/eventlist.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/public.sdk/source/vst/hosting/hostclasses.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/public.sdk/source/vst/hosting/module.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/public.sdk/source/vst/hosting/module_win32.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/public.sdk/source/vst/hosting/parameterchanges.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/public.sdk/source/vst/hosting/pluginterfacesupport.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/public.sdk/source/vst/hosting/plugprovider.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/public.sdk/source/vst/hosting/processdata.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/public.sdk/source/vst/utility/stringconvert.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/public.sdk/source/vst/vstinitiids.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/public.sdk/source/common/threadchecker_win32.cpp

    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/pluginterfaces/base/conststringtable.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/pluginterfaces/base/coreiids.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/pluginterfaces/base/funknown.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/base/source/fobject.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/base/source/fdebug.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/base/source/updatehandler.cpp
    ${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/base/thread/source/flock.cpp
)

target_include_directories(VST_SDK
    PUBLIC "${VSTModuleArchitectureSDK_SOURCE_DIR}/vst3sdk/"
)

target_compile_definitions(VST_SDK
    PUBLIC
        "-DRELEASE"
)

CPMAddPackage(
    NAME SDL2
    VERSION  2.0.22
    URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.0.22.zip
    OPTIONS
        "SDL_SHARED Off"
)

CPMAddPackage(
    NAME portaudio
    GITHUB_REPOSITORY "PortAudio/portaudio"
    GIT_TAG v19.7.0
    GIT_SHALLOW ON
    OPTIONS
        "PA_BUILD_TESTS Off"
        "PA_BUILD_EXAMPLES Off"
)

CPMAddPackage(
    NAME RtMidi
    GITHUB_REPOSITORY "thestk/rtmidi"
    GIT_TAG 5.0.0
    DOWNLOAD_ONLY True
    OPTIONS
        "PA_BUILD_TESTS Off"
        "PA_BUILD_EXAMPLES Off"
)

add_library(RtMidi
    "${RtMidi_SOURCE_DIR}/RtMidi.cpp"
)

target_include_directories(RtMidi PUBLIC "${RtMidi_SOURCE_DIR}")

target_compile_definitions(RtMidi
    PUBLIC
        "-D__WINDOWS_MM__"
)

CPMAddPackage(
    NAME concurrentqueue
    GITHUB_REPOSITORY cameron314/concurrentqueue
    GIT_TAG v1.0.3
    DOWNLOAD_ONLY True
)

if(concurrentqueue_ADDED)
    add_library(concurrentqueue INTERFACE IMPORTED)
    target_include_directories(concurrentqueue INTERFACE "${concurrentqueue_SOURCE_DIR}")
endif()

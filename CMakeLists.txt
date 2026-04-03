cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(vo_studio LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(wxBUILD_SHARED OFF)

message(STATUS "Fetching wxWidgets...")

FetchContent_Declare(
   wxWidgets
   GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
   GIT_SHALLOW ON
)
FetchContent_MakeAvailable(wxWidgets)

# Find libav (FFmpeg) for audio processing
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED libavformat libavcodec libswresample libavutil)

# miniaudio for audio recording/playback
FetchContent_Declare(
    miniaudio
    URL https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(miniaudio)

set(SRCS
    src/main.cpp
    src/app.cpp
    src/app.h
    src/frame.cpp
    src/frame.h
    src/workspace.cpp
    src/workspace.h
    src/text_panel.cpp
    src/text_panel.h
    src/audio_list_panel.cpp
    src/audio_list_panel.h
    src/audio_engine.cpp
    src/audio_engine.h
    src/lufs_analyzer.cpp
    src/lufs_analyzer.h
)

if(APPLE)
    add_executable(vo_studio MACOSX_BUNDLE ${SRCS})
    set_target_properties(vo_studio PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
elseif(WIN32)
    add_executable(vo_studio WIN32 ${SRCS} main.exe.manifest resources.rc)
else()
    add_executable(vo_studio ${SRCS})
endif()

target_include_directories(vo_studio PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LIBAV_INCLUDE_DIRS}
    ${miniaudio_SOURCE_DIR}
)

target_link_libraries(vo_studio PRIVATE 
    wxcore 
    wxnet
    ${LIBAV_LIBRARIES}
)

target_compile_options(vo_studio PRIVATE ${LIBAV_CFLAGS_OTHER})

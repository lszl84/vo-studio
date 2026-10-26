cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(vo_studio LANGUAGES CXX)

include(FetchContent)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use the system wxWidgets (e.g. libwxgtk3.2-dev on Debian)
find_package(wxWidgets 3.2 REQUIRED COMPONENTS net core base)
include(${wxWidgets_USE_FILE})

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

# nlohmann/json for project metadata
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

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
    ${wxWidgets_LIBRARIES}
    nlohmann_json::nlohmann_json
    ${LIBAV_LIBRARIES}
)

target_compile_options(vo_studio PRIVATE ${LIBAV_CFLAGS_OTHER})

if(UNIX AND NOT APPLE)
    install(TARGETS vo_studio RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/vo-studio.desktop.in
                   ${CMAKE_CURRENT_BINARY_DIR}/vo-studio.desktop @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/vo-studio.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/vo-studio.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()

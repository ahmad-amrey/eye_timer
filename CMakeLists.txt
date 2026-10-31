cmake_minimum_required(VERSION 3.21)

project(EyeTimer VERSION 2.0.0 DESCRIPTION "Eye-rest reminder" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EYETIMER_WITH_MULTIMEDIA "Play tones through Qt Multimedia when it is installed" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
qt_standard_project_setup()

if(EYETIMER_WITH_MULTIMEDIA)
    find_package(Qt6 QUIET COMPONENTS Multimedia)
endif()
if(UNIX AND NOT APPLE)
    find_package(Qt6 QUIET COMPONENTS DBus)
endif()

set(EYETIMER_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Platform services with no UI: settings, idle detection, login items, tones.
qt_add_library(eyetimer_core STATIC
    src/Autostart.cpp
    src/Autostart.h
    src/Config.cpp
    src/Config.h
    src/IdleDetector.cpp
    src/IdleDetector.h
    src/Platform.cpp
    src/Platform.h
    src/SoundPlayer.cpp
    src/SoundPlayer.h
)
target_include_directories(eyetimer_core PUBLIC src)
target_link_libraries(eyetimer_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)
target_compile_definitions(eyetimer_core PRIVATE EYETIMER_ASSET_DIR="${EYETIMER_ASSET_DIR}")

if(TARGET Qt6::Multimedia)
    target_link_libraries(eyetimer_core PUBLIC Qt6::Multimedia)
    target_compile_definitions(eyetimer_core PUBLIC EYETIMER_HAS_MULTIMEDIA)
    message(STATUS "Eye Timer: tones play through Qt Multimedia")
else()
    message(STATUS "Eye Timer: Qt Multimedia not found, tones use the platform player")
endif()

if(TARGET Qt6::DBus)
    target_link_libraries(eyetimer_core PUBLIC Qt6::DBus)
    target_compile_definitions(eyetimer_core PUBLIC EYETIMER_HAS_DBUS)
endif()

if(WIN32)
    target_link_libraries(eyetimer_core PUBLIC shell32 ole32 uuid winmm)
    target_compile_definitions(eyetimer_core PUBLIC UNICODE _UNICODE NOMINMAX)
elseif(APPLE)
    target_link_libraries(eyetimer_core PUBLIC "-framework IOKit" "-framework CoreFoundation")
elseif(UNIX)
    target_link_libraries(eyetimer_core PUBLIC ${CMAKE_DL_LIBS})
endif()

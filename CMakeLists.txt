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

# The app: soft light UI, timer logic, tray icon.
set(EYETIMER_ASSETS
    "${EYETIMER_ASSET_DIR}/icon.ico"
    "${EYETIMER_ASSET_DIR}/icon.png"
    "${EYETIMER_ASSET_DIR}/universfield-new-notification-017-352293.mp3"
    "${EYETIMER_ASSET_DIR}/universfield-new-notification-017-352293-pitch-changed.mp3"
)

set(EYETIMER_UI_SOURCES
    src/BreakOverlay.cpp
    src/BreakOverlay.h
    src/CountdownWindow.cpp
    src/CountdownWindow.h
    src/EyeTimerApp.cpp
    src/EyeTimerApp.h
    src/main.cpp
    src/PreparePanel.cpp
    src/PreparePanel.h
    src/SettingsWindow.cpp
    src/SettingsWindow.h
    src/Theme.cpp
    src/Theme.h
    src/Widgets.cpp
    src/Widgets.h
)

if(WIN32)
    configure_file(packaging/windows/eye_timer.rc.in "${CMAKE_CURRENT_BINARY_DIR}/eye_timer.rc" @ONLY)
    list(APPEND EYETIMER_UI_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/eye_timer.rc")
elseif(APPLE)
    list(APPEND EYETIMER_UI_SOURCES ${EYETIMER_ASSETS})
    set_source_files_properties(${EYETIMER_ASSETS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

qt_add_executable(EyeTimer WIN32 MACOSX_BUNDLE ${EYETIMER_UI_SOURCES})
target_link_libraries(EyeTimer PRIVATE eyetimer_core)
target_compile_definitions(EyeTimer PRIVATE EYETIMER_VERSION="${PROJECT_VERSION}")

if(APPLE)
    set_target_properties(EyeTimer PROPERTIES
        OUTPUT_NAME "Eye Timer"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.eyetimer.app"
        MACOSX_BUNDLE_BUNDLE_NAME "Eye Timer"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
else()
    # Icons and tones sit next to the executable, as the app looks for them there.
    add_custom_command(TARGET EyeTimer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EYETIMER_ASSETS} "$<TARGET_FILE_DIR:EyeTimer>"
        VERBATIM
    )
endif()

include(GNUInstallDirs)
if(WIN32)
    install(TARGETS EyeTimer RUNTIME DESTINATION .)
    install(FILES ${EYETIMER_ASSETS} DESTINATION .)
elseif(APPLE)
    install(TARGETS EyeTimer BUNDLE DESTINATION .)
else()
    install(TARGETS EyeTimer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES ${EYETIMER_ASSETS} DESTINATION ${CMAKE_INSTALL_DATADIR}/eye-timer)
    install(FILES packaging/linux/eye-timer.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES "${EYETIMER_ASSET_DIR}/icon.png" DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
            RENAME eye-timer.png)
endif()

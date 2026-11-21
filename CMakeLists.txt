cmake_minimum_required(VERSION 3.21)

project(EyeTimer VERSION 0.10.3 DESCRIPTION "Eye-rest reminder" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows plays tones with PlaySound, so Qt Multimedia only matters elsewhere.
option(EYETIMER_WITH_MULTIMEDIA "Play tones through Qt Multimedia when it is installed (macOS/Linux)" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network)
qt_standard_project_setup()

if(EYETIMER_WITH_MULTIMEDIA AND NOT WIN32)
    find_package(Qt6 QUIET COMPONENTS Multimedia)
endif()
if(UNIX AND NOT APPLE)
    find_package(Qt6 QUIET COMPONENTS DBus)
endif()

set(EYETIMER_ASSET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")

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
    src/Schedule.cpp
    src/Schedule.h
    src/SoundPlayer.cpp
    src/SoundPlayer.h
    src/UpdateChecker.cpp
    src/UpdateChecker.h
    src/Version.cpp
    src/Version.h
)
target_include_directories(eyetimer_core PUBLIC src)
target_link_libraries(eyetimer_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
target_compile_definitions(eyetimer_core PRIVATE EYETIMER_ASSET_DIR="${EYETIMER_ASSET_DIR}")

if(WIN32)
    message(STATUS "Eye Timer: tones play through Media Foundation and PlaySound")
elseif(TARGET Qt6::Multimedia)
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
    target_link_libraries(eyetimer_core PUBLIC advapi32 shell32 shlwapi ole32 uuid winmm mfplat mfreadwrite mfuuid)
    target_compile_definitions(eyetimer_core PUBLIC UNICODE _UNICODE NOMINMAX)
elseif(APPLE)
    enable_language(OBJCXX)
    target_sources(eyetimer_core PRIVATE src/PlatformMac.mm)
    target_link_libraries(eyetimer_core PUBLIC "-framework IOKit" "-framework CoreFoundation" "-framework AppKit")
elseif(UNIX)
    target_link_libraries(eyetimer_core PUBLIC ${CMAKE_DL_LIBS})
endif()

# The app: soft light UI, timer logic, tray icon.

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
    src/UpdateDialog.cpp
    src/UpdateDialog.h
    src/Widgets.cpp
    src/Widgets.h
)

if(WIN32)
    configure_file(packaging/windows/eye_timer.rc.in "${CMAKE_CURRENT_BINARY_DIR}/eye_timer.rc" @ONLY)
    list(APPEND EYETIMER_UI_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/eye_timer.rc")
elseif(APPLE)
    # Bundle icon, generated from the PNG with the macOS tools.
    set(EYETIMER_ICNS "${CMAKE_CURRENT_BINARY_DIR}/EyeTimer.icns")
    add_custom_command(
        OUTPUT "${EYETIMER_ICNS}"
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE=${EYETIMER_ASSET_DIR}/icon.png" "-DOUTPUT=${EYETIMER_ICNS}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/make_icns.cmake"
        DEPENDS "${EYETIMER_ASSET_DIR}/icon.png" packaging/macos/make_icns.cmake
        VERBATIM
    )
    list(APPEND EYETIMER_UI_SOURCES "${EYETIMER_ICNS}")
    set_source_files_properties("${EYETIMER_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

qt_add_executable(EyeTimer WIN32 MACOSX_BUNDLE ${EYETIMER_UI_SOURCES})
target_link_libraries(EyeTimer PRIVATE eyetimer_core)
target_compile_definitions(EyeTimer PRIVATE EYETIMER_VERSION="${PROJECT_VERSION}")


# Icon and tones live inside the executable, so every package ships one file.
qt_add_resources(EyeTimer eyetimer_assets
    PREFIX "/eyetimer"
    BASE "${EYETIMER_ASSET_DIR}"
    FILES
        "${EYETIMER_ASSET_DIR}/icon.png"
        "${EYETIMER_ASSET_DIR}/universfield-new-notification-017-352293.mp3"
        "${EYETIMER_ASSET_DIR}/universfield-new-notification-017-352293-pitch-changed.mp3"
)

if(APPLE)
    set_target_properties(EyeTimer PROPERTIES
        OUTPUT_NAME "Eye Timer"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.eyetimer.app"
        MACOSX_BUNDLE_BUNDLE_NAME "Eye Timer"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "EyeTimer.icns"
    )
endif()

include(GNUInstallDirs)
if(WIN32)
    install(TARGETS EyeTimer RUNTIME DESTINATION .)
elseif(APPLE)
    install(TARGETS EyeTimer BUNDLE DESTINATION .)
else()
    install(TARGETS EyeTimer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES packaging/linux/eye-timer.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES "${EYETIMER_ASSET_DIR}/icon.png" DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
            RENAME eye-timer.png)
endif()

# Linux package: `cpack --config build/CPackConfig.cmake` builds a .deb that
# installs to /usr. User settings in ~/.local/share/EyeTimer are never touched
# by installing, upgrading or removing it.
if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "eye-timer")
    set(CPACK_PACKAGE_VENDOR "Ahmad AlAmrey")
    set(CPACK_PACKAGE_CONTACT "Ahmad AlAmrey <ahmad-amrey@users.noreply.github.com>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Eye-rest reminder following the 20-20-20 rule")
    set(CPACK_PACKAGE_DESCRIPTION
        "Eye Timer lives in the system tray and reminds you to rest your eyes: after a stretch of active "
        "computer use it shows a full-screen rest overlay with a short countdown and a healthy suggestion. "
        "Idle time pauses the timer, and a long absence counts as a rest.")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/ahmad-amrey/eye_timer")
    # Qt and system libraries are found by dpkg-shlibdeps; GStreamer's plugins
    # are what Qt Multimedia uses to decode and play the MP3 tones.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "gstreamer1.0-plugins-base, gstreamer1.0-plugins-good")
    set(CPACK_DEBIAN_PACKAGE_SUGGESTS "wmctrl")
    include(CPack)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

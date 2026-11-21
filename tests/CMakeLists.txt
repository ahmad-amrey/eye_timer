function(eyetimer_add_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE eyetimer_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

eyetimer_add_test(tst_version)
eyetimer_add_test(tst_config)
eyetimer_add_test(tst_schedule)
eyetimer_add_test(tst_updatechecker)
eyetimer_add_test(tst_autostart)

if(WIN32)
    # Decodes the embedded tones exactly as the app does before playing them.
    eyetimer_add_test(tst_sound)
    qt_add_resources(tst_sound tst_sound_assets
        PREFIX "/eyetimer"
        BASE "${EYETIMER_ASSET_DIR}"
        FILES
            "${EYETIMER_ASSET_DIR}/universfield-new-notification-017-352293.mp3"
            "${EYETIMER_ASSET_DIR}/universfield-new-notification-017-352293-pitch-changed.mp3"
    )
endif()

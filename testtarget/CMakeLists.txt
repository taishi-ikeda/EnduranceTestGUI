# Minimal target application used to exercise EnduranceTestGUI against.
# Built as a separate executable; see ../src for the actual test tool.

add_executable(TestTarget
    src/main.cpp
    src/TestTargetWindow.cpp
    src/TestTargetWindow.h
    src/CounterButton.cpp
    src/CounterButton.h
)

target_link_libraries(TestTarget PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

if(APPLE)
    set_target_properties(TestTarget PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    )
    # Ad-hoc sign so the app has a stable identity; macOS's Accessibility
    # (TCC) permission bookkeeping is unreliable for completely unsigned
    # bundles, and CGWindowList/AX APIs used to find and control this
    # window can behave inconsistently otherwise. Note this is TestTarget
    # itself that's signed here -- TestTarget never needs Accessibility
    # permission (only EnduranceTestGUI, the controller, does); this just
    # makes it a well-formed, reliably-identifiable target app.
    add_custom_command(TARGET TestTarget POST_BUILD
        COMMAND codesign --force --deep --sign - $<TARGET_BUNDLE_DIR:TestTarget>
        COMMENT "Ad-hoc code-signing TestTarget.app"
    )
endif()

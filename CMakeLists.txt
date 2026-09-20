cmake_minimum_required(VERSION 3.20)
project(EnduranceTestGUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_PREFIX_PATH)
    # Prefer a Qt Online Installer install under ~/Qt (its macOS build is a
    # universal binary, x86_64 + arm64, so the app can run natively on
    # Apple Silicon -- important because a Rosetta-translated (x86_64-only)
    # build has been observed to have unreliable macOS Accessibility (TCC)
    # permission recognition). Falls back to Homebrew's Qt at /usr/local
    # (Intel prefix; x86_64-only, so the app runs under Rosetta there).
    if(APPLE)
    	file(GLOB _qt_installer_dirs "$ENV{HOME}/Qt/*/macos")
    elseif(UNIX)
    	file(GLOB _qt_installer_dirs "$ENV{HOME}/Qt/*/gcc_64")
    endif()
    if(_qt_installer_dirs)
        list(SORT _qt_installer_dirs)
        list(GET _qt_installer_dirs -1 _qt_installer_latest)
        set(CMAKE_PREFIX_PATH "${_qt_installer_latest}/lib/cmake")
    else()
        set(CMAKE_PREFIX_PATH "/usr/local/lib/cmake")
    endif()
    message(STATUS "Auto-detected CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
endif()

# If the resolved Qt is x86_64-only (e.g. Homebrew's Intel prefix), match
# it so the app links; this makes the whole app run under Rosetta on
# Apple Silicon. Prefer a universal/arm64 Qt (see above) when available.
# Override with -DCMAKE_OSX_ARCHITECTURES=arm64 to force native even with
# an x86_64-only Qt (linking will fail unless Qt is actually universal).
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    execute_process(COMMAND file "${CMAKE_PREFIX_PATH}/Qt6/../../QtCore.framework/QtCore"
        OUTPUT_VARIABLE _qt_file_output ERROR_QUIET)
    if(_qt_file_output MATCHES "x86_64" AND NOT _qt_file_output MATCHES "arm64")
        set(CMAKE_OSX_ARCHITECTURES "x86_64")
    endif()
endif()

# Support building against either Qt5 or Qt6 (whichever is found first).
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
message(STATUS "Using Qt${QT_VERSION_MAJOR} (${QT_VERSION})")

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/RandomActionEngine.cpp
    src/RegionSelectorOverlay.cpp
    src/RegionHighlightOverlay.cpp
    src/StopPanel.cpp
    src/StepEditorDialog.cpp
    src/ActionParamsEditor.cpp
    src/NamedRegionEditorDialog.cpp
    src/DefaultActionParamsDialog.cpp
    src/ActionKindEditor.cpp
    src/TestConfigJson.cpp
)

set(HEADERS
    src/MainWindow.h
    src/RandomActionEngine.h
    src/RegionSelectorOverlay.h
    src/RegionHighlightOverlay.h
    src/StopPanel.h
    src/StepEditorDialog.h
    src/ActionParamsEditor.h
    src/NamedRegionEditorDialog.h
    src/DefaultActionParamsDialog.h
    src/ActionKindEditor.h
    src/TestConfig.h
    src/TestConfigJson.h
)

list(APPEND HEADERS src/platform/PlatformAutomation.h src/platform/GlobalHotkey.h)

if(APPLE)
    enable_language(OBJCXX)
    list(APPEND SOURCES src/platform/macos/Automation_mac.mm src/platform/macos/GlobalHotkey_mac.mm)
    set_source_files_properties(src/platform/macos/Automation_mac.mm src/platform/macos/GlobalHotkey_mac.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
elseif(UNIX)
    list(APPEND SOURCES src/platform/linux/Automation_linux.cpp src/platform/linux/GlobalHotkey_linux.cpp)
endif()

add_executable(EnduranceTestGUI ${SOURCES} ${HEADERS})

target_include_directories(EnduranceTestGUI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(EnduranceTestGUI PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

if(APPLE)
    find_library(APPLICATIONSERVICES_LIB ApplicationServices)
    find_library(COCOA_LIB Cocoa)
    find_library(CARBON_LIB Carbon)
    target_link_libraries(EnduranceTestGUI PRIVATE
        ${APPLICATIONSERVICES_LIB}
        ${COCOA_LIB}
        ${CARBON_LIB}
    )
    set_target_properties(EnduranceTestGUI PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    )
    # Ad-hoc sign so the app has a stable identity for macOS's Accessibility
    # (TCC) permission bookkeeping. A completely unsigned bundle can fail to
    # register properly in System Settings > Privacy & Security >
    # Accessibility (the permission this app needs to control other
    # processes) or can silently lose a previously-granted permission.
    add_custom_command(TARGET EnduranceTestGUI POST_BUILD
        COMMAND codesign --force --deep --sign - $<TARGET_BUNDLE_DIR:EnduranceTestGUI>
        COMMENT "Ad-hoc code-signing EnduranceTestGUI.app"
    )
elseif(UNIX)
    find_package(X11 REQUIRED)
    if(NOT X11_XTest_FOUND)
        message(FATAL_ERROR "XTest extension development files not found (libxtst-dev / libXtst-devel)")
    endif()
    target_include_directories(EnduranceTestGUI PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(EnduranceTestGUI PRIVATE
        ${X11_LIBRARIES}
        ${X11_XTest_LIB}
        gobject-2.0
    )

    # Optional: AT-SPI powers the experimental "select a context-menu item"
    # feature on Linux. Without it, that feature is a no-op (menus just get
    # dismissed) but everything else still works.
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(ATSPI atspi-2)
    endif()
    if(ATSPI_FOUND)
        target_compile_definitions(EnduranceTestGUI PRIVATE HAVE_ATSPI=1)
        target_include_directories(EnduranceTestGUI PRIVATE ${ATSPI_INCLUDE_DIRS})
        target_link_libraries(EnduranceTestGUI PRIVATE ${ATSPI_LIBRARIES})
    else()
        message(WARNING "atspi-2 not found; context-menu item selection will be a no-op on Linux. "
            "Install libatspi2.0-dev (Debian/Ubuntu) or at-spi2-core-devel (Fedora) to enable it.")
    endif()
endif()

# Minimal target app for manually exercising EnduranceTestGUI against.
add_subdirectory(testtarget)

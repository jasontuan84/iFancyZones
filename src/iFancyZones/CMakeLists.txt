cmake_minimum_required(VERSION 3.21)

# Must be set before project(): project() creates an empty cache entry, and a
# plain set(... CACHE) never overrides an existing entry, so the binary would
# silently target the build machine's SDK. FORCE an empty (stale) entry so old
# build dirs pick it up too; an explicit -D value still wins.
# Qt 6.12 itself is built for macOS 14.4 and later, so nothing older can run.
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.4" CACHE STRING "Minimum macOS" FORCE)
endif()

project(iFancyZones
    VERSION 1.7.0
    DESCRIPTION "Zone-based window manager for macOS"
    LANGUAGES CXX OBJCXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
endif()

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Widgets Svg)
qt_standard_project_setup()
# qt_standard_project_setup() turns on AUTOMOC and AUTOUIC only. The .qrc in
# the sources needs AUTORCC, or ":/icons/..." resources are never embedded.
set(CMAKE_AUTORCC ON)

set(IFZ_CORE_SOURCES
    core/Zone.h core/Zone.cpp
    core/Layout.h core/Layout.cpp
    core/Monitor.h core/Monitor.cpp
    core/LayoutEngine.h core/LayoutEngine.cpp
)

set(IFZ_SERVICES_SOURCES
    services/SettingsStore.h services/SettingsStore.cpp
    services/AppSettings.h services/AppSettings.cpp
    services/WindowManager.h services/WindowManager.cpp
)

set(IFZ_APP_SOURCES
    app/Application.h app/Application.cpp
    app/TrayController.h app/TrayController.cpp
    app/AppSwitcher.h app/AppSwitcher.cpp
)

set(IFZ_UI_SOURCES
    ui/PopoverTheme.h ui/PopoverTheme.cpp
    ui/PopoverWidgets.h ui/PopoverWidgets.cpp
    ui/TrayPopover.h ui/TrayPopover.cpp
    ui/ScreenLayoutTab.h ui/ScreenLayoutTab.cpp
    ui/LayoutsTab.h ui/LayoutsTab.cpp
    ui/LayoutEditor.h ui/LayoutEditor.cpp
    ui/EditorToolbar.h ui/EditorToolbar.cpp
    ui/ZoneEditorWidget.h ui/ZoneEditorWidget.cpp
    ui/ZoneOverlay.h ui/ZoneOverlay.cpp
    ui/CarouselOverlay.h ui/CarouselOverlay.cpp
    ui/SettingsWindow.h ui/SettingsWindow.cpp
    ui/HotkeyEdit.h ui/HotkeyEdit.cpp
    ui/ChordEdit.h ui/ChordEdit.cpp
    ui/ScreenNumberBadge.h ui/ScreenNumberBadge.cpp
)

set(IFZ_MAC_SOURCES
    platform/mac/AccessibilityBridge.h
    platform/mac/AccessibilityBridge.mm
    platform/mac/DragDetector.h
    platform/mac/DragDetector.mm
    platform/mac/AppKitBridge.h
    platform/mac/AppKitBridge.mm
    platform/mac/MacScreenInfo.h
    platform/mac/MacScreenInfo.mm
    platform/mac/HotkeyManager.h
    platform/mac/HotkeyManager.mm
    platform/mac/AppSwitcherBridge.h
    platform/mac/AppSwitcherBridge.mm
    platform/mac/SwitcherInput.h
    platform/mac/SwitcherInput.mm
    platform/mac/LoginItem.h
    platform/mac/LoginItem.mm
)

qt_add_executable(iFancyZones
    MACOSX_BUNDLE
    main.cpp
    ${IFZ_CORE_SOURCES}
    ${IFZ_SERVICES_SOURCES}
    ${IFZ_APP_SOURCES}
    ${IFZ_UI_SOURCES}
    ${IFZ_MAC_SOURCES}
    resources/iFancyZones.qrc
)

set(IFZ_MAC_MM_SOURCES
    platform/mac/AccessibilityBridge.mm
    platform/mac/DragDetector.mm
    platform/mac/AppKitBridge.mm
    platform/mac/MacScreenInfo.mm
    platform/mac/HotkeyManager.mm
    platform/mac/AppSwitcherBridge.mm
    platform/mac/SwitcherInput.mm
    platform/mac/LoginItem.mm
)
set_source_files_properties(${IFZ_MAC_MM_SOURCES} PROPERTIES
    LANGUAGE OBJCXX
    COMPILE_FLAGS "-fobjc-arc"
)

target_include_directories(iFancyZones PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(iFancyZones PRIVATE
    IFZ_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(iFancyZones PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    "-framework AppKit"
    "-framework ApplicationServices"
    "-framework Carbon"
    "-framework CoreGraphics"
    "-framework QuartzCore"
    "-framework CoreFoundation"
    "-framework Foundation"
    "-framework ServiceManagement"
)

set_target_properties(iFancyZones PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "iFancyZones"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.tuanquynh.ifancyzones"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "Copyright 2026 Jason"
    MACOSX_BUNDLE_ICON_FILE "iFancyZones.icns"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in"
)

# Embed the .icns into the bundle's Resources/ folder.
set(IFZ_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/iFancyZones.icns")
target_sources(iFancyZones PRIVATE "${IFZ_ICNS}")
set_source_files_properties("${IFZ_ICNS}" PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources"
)

install(TARGETS iFancyZones
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET iFancyZones
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

cmake_minimum_required(VERSION 3.21)

# Version is injected by CI from the git tag (e.g. -DANYPC_VERSION=1.2.3).
set(ANYPC_VERSION "1.1.0" CACHE STRING "Application version (MAJOR.MINOR.PATCH)")
set(ANYPC_DEFAULT_SERVER_URL "" CACHE STRING "Optional backend URL suggested on first launch (users always confirm it)")

project(AnyPC
    VERSION ${ANYPC_VERSION}
    DESCRIPTION "Cross-platform remote desktop client"
    HOMEPAGE_URL "https://github.com/sefaun/any-pc"
    LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network WebSockets WebChannel WebEngineCore WebEngineWidgets)

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.5)
    qt_standard_project_setup()
endif()

configure_file(src/app/BuildInfo.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/BuildInfo.h @ONLY)

option(ANYPC_BUILD_TESTS "Build the unit tests" ON)

# ---------------------------------------------------------------------------
# anypc_lib: all application code, shared by the executable and the tests.
# ---------------------------------------------------------------------------
set(ANYPC_LIB_SOURCES
    src/app/Application.cpp            src/app/Application.h
    src/core/AppSettings.cpp           src/core/AppSettings.h
    src/core/DeviceId.cpp              src/core/DeviceId.h
    src/core/Logging.cpp               src/core/Logging.h
    src/core/PasswordHasher.cpp        src/core/PasswordHasher.h
    src/core/RecentConnections.cpp     src/core/RecentConnections.h
    src/core/ScreenCapturePermission.h
    src/network/ApiClient.cpp          src/network/ApiClient.h
    src/network/SignalingClient.cpp    src/network/SignalingClient.h
    src/network/Protocol.h
    src/rtc/RtcBridge.cpp              src/rtc/RtcBridge.h
    src/rtc/RtcPage.cpp                src/rtc/RtcPage.h
    src/rtc/WebEngineEnvironment.cpp   src/rtc/WebEngineEnvironment.h
    src/session/ClipboardSync.cpp      src/session/ClipboardSync.h
    src/session/RtcSession.cpp         src/session/RtcSession.h
    src/session/HostSession.cpp        src/session/HostSession.h
    src/session/ViewerSession.cpp      src/session/ViewerSession.h
    src/session/SessionManager.cpp     src/session/SessionManager.h
    src/session/ScreenMatcher.cpp      src/session/ScreenMatcher.h
    src/transfer/FileTransferManager.cpp src/transfer/FileTransferManager.h
    src/input/InputEvent.cpp           src/input/InputEvent.h
    src/input/InputInjector.cpp        src/input/InputInjector.h
    src/input/KeyCodeMap.cpp           src/input/KeyCodeMap.h
    src/ui/MainWindow.cpp              src/ui/MainWindow.h
    src/ui/IncomingRequestDialog.cpp   src/ui/IncomingRequestDialog.h
    src/ui/SettingsDialog.cpp          src/ui/SettingsDialog.h
    src/ui/ServerSetupDialog.cpp       src/ui/ServerSetupDialog.h
    src/ui/ViewerWindow.cpp            src/ui/ViewerWindow.h
    src/ui/FileTransferDialog.cpp      src/ui/FileTransferDialog.h
    src/ui/ScreenBorderOverlay.cpp     src/ui/ScreenBorderOverlay.h
    src/ui/RecentConnectionsView.cpp   src/ui/RecentConnectionsView.h
    src/ui/Theme.cpp                   src/ui/Theme.h
    src/ui/QrCodeWidget.cpp            src/ui/QrCodeWidget.h
)

# Third party code is compiled without the project's warning flags.
add_library(anypc_qrcodegen STATIC src/third_party/qrcodegen/qrcodegen.cpp src/third_party/qrcodegen/qrcodegen.hpp)
set_target_properties(anypc_qrcodegen PROPERTIES AUTOMOC OFF)

if(WIN32)
    list(APPEND ANYPC_LIB_SOURCES src/input/platform/WindowsInputInjector.cpp src/core/ScreenCapturePermission.cpp)
elseif(APPLE)
    list(APPEND ANYPC_LIB_SOURCES src/input/platform/MacInputInjector.mm src/core/ScreenCapturePermission.mm)
else()
    list(APPEND ANYPC_LIB_SOURCES src/input/platform/X11InputInjector.cpp src/core/ScreenCapturePermission.cpp)
endif()

add_library(anypc_lib STATIC ${ANYPC_LIB_SOURCES})
target_include_directories(anypc_lib PUBLIC src ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_definitions(anypc_lib PUBLIC
    QT_NO_KEYWORDS
    QT_USE_QSTRINGBUILDER
    QT_DISABLE_DEPRECATED_UP_TO=0x060000)
if(MSVC)
    target_compile_options(anypc_lib PUBLIC /W4 /permissive- /utf-8)
else()
    target_compile_options(anypc_lib PUBLIC -Wall -Wextra -Wpedantic)
endif()
target_link_libraries(anypc_lib PUBLIC
    anypc_qrcodegen
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::WebSockets
    Qt6::WebChannel Qt6::WebEngineCore Qt6::WebEngineWidgets)

if(WIN32)
    target_link_libraries(anypc_lib PUBLIC user32)
elseif(APPLE)
    target_link_libraries(anypc_lib PUBLIC "-framework ApplicationServices" "-framework CoreGraphics" "-framework Carbon")
else()
    find_package(X11 REQUIRED)
    if(NOT X11_XTest_FOUND)
        message(FATAL_ERROR "libXtst development files are required (e.g. libxtst-dev)")
    endif()
    target_link_libraries(anypc_lib PUBLIC X11::X11 X11::Xtst)
endif()

# ---------------------------------------------------------------------------
# anypc: the application.
# ---------------------------------------------------------------------------
set(ANYPC_APP_SOURCES src/main.cpp resources/resources.qrc)
if(WIN32)
    list(APPEND ANYPC_APP_SOURCES packaging/windows/anypc.rc)
elseif(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE AnyPC.icns)
    set(ANYPC_ICNS ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/AnyPC.icns)
    set_source_files_properties(${ANYPC_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND ANYPC_APP_SOURCES ${ANYPC_ICNS})
endif()

qt_add_executable(anypc WIN32 MACOSX_BUNDLE ${ANYPC_APP_SOURCES})
target_link_libraries(anypc PRIVATE anypc_lib)

if(WIN32)
    set_target_properties(anypc PROPERTIES OUTPUT_NAME "AnyPC")
elseif(APPLE)
    set_target_properties(anypc PROPERTIES
        OUTPUT_NAME "AnyPC"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.anypc.desktop"
        MACOSX_BUNDLE_BUNDLE_NAME "AnyPC"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
endif()

if(ANYPC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(TARGETS anypc
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/anypc.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES resources/icons/anypc.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    install(FILES resources/icons/anypc-256.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps RENAME anypc.png)
endif()

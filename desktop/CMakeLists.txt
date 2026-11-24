cmake_minimum_required(VERSION 3.21)

# Version is injected by CI from the git tag (e.g. -DANYPC_VERSION=1.2.3).
set(ANYPC_VERSION "0.1.0" CACHE STRING "Application version (MAJOR.MINOR.PATCH)")
set(ANYPC_DEFAULT_SERVER_URL "http://localhost:3000" CACHE STRING "Default backend URL baked into the client")

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

set(ANYPC_SOURCES
    src/main.cpp
    src/app/Application.cpp            src/app/Application.h
    src/core/AppSettings.cpp           src/core/AppSettings.h
    src/core/DeviceId.cpp              src/core/DeviceId.h
    src/core/PasswordHasher.cpp        src/core/PasswordHasher.h
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
    src/input/InputEvent.cpp           src/input/InputEvent.h
    src/input/InputInjector.cpp        src/input/InputInjector.h
    src/input/KeyCodeMap.cpp           src/input/KeyCodeMap.h
    src/ui/MainWindow.cpp              src/ui/MainWindow.h
    src/ui/IncomingRequestDialog.cpp   src/ui/IncomingRequestDialog.h
    src/ui/SettingsDialog.cpp          src/ui/SettingsDialog.h
    src/ui/ViewerWindow.cpp            src/ui/ViewerWindow.h
    src/ui/Theme.cpp                   src/ui/Theme.h
    resources/resources.qrc
)

if(WIN32)
    list(APPEND ANYPC_SOURCES src/input/platform/WindowsInputInjector.cpp packaging/windows/anypc.rc)
elseif(APPLE)
    list(APPEND ANYPC_SOURCES src/input/platform/MacInputInjector.mm)
    set(MACOSX_BUNDLE_ICON_FILE AnyPC.icns)
    set(ANYPC_ICNS ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/AnyPC.icns)
    set_source_files_properties(${ANYPC_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND ANYPC_SOURCES ${ANYPC_ICNS})
else()
    list(APPEND ANYPC_SOURCES src/input/platform/X11InputInjector.cpp)
endif()

qt_add_executable(anypc WIN32 MACOSX_BUNDLE ${ANYPC_SOURCES})

target_include_directories(anypc PRIVATE src ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_compile_definitions(anypc PRIVATE
    QT_NO_KEYWORDS
    QT_USE_QSTRINGBUILDER
    QT_DISABLE_DEPRECATED_UP_TO=0x060000)

if(MSVC)
    target_compile_options(anypc PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(anypc PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(anypc PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::WebSockets
    Qt6::WebChannel Qt6::WebEngineCore Qt6::WebEngineWidgets)

if(WIN32)
    target_link_libraries(anypc PRIVATE user32)
    set_target_properties(anypc PROPERTIES OUTPUT_NAME "AnyPC")
elseif(APPLE)
    target_link_libraries(anypc PRIVATE "-framework ApplicationServices" "-framework CoreGraphics" "-framework Carbon")
    set_target_properties(anypc PROPERTIES
        OUTPUT_NAME "AnyPC"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.anypc.desktop"
        MACOSX_BUNDLE_BUNDLE_NAME "AnyPC"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
else()
    find_package(X11 REQUIRED)
    if(NOT X11_XTest_FOUND)
        message(FATAL_ERROR "libXtst development files are required (e.g. libxtst-dev)")
    endif()
    target_link_libraries(anypc PRIVATE X11::X11 X11::Xtst)
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

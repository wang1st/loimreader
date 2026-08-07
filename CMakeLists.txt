cmake_minimum_required(VERSION 3.25)

# The C17 rewrite is the default build.  The Qt application remains available
# during the migration with -DLOIM_BUILD_LEGACY_QT=ON, but is deliberately kept
# out of the release path.
option(LOIM_BUILD_LEGACY_QT "Build the legacy Qt application" OFF)

if(NOT LOIM_BUILD_LEGACY_QT)
    set(LOIM_VERSION_OVERRIDE "3.0.0" CACHE STRING "C17 application version")
    string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" LOIM_NUMERIC_VERSION "${LOIM_VERSION_OVERRIDE}")
    if(NOT LOIM_NUMERIC_VERSION)
        message(FATAL_ERROR "LOIM_VERSION_OVERRIDE must start with major.minor.patch")
    endif()
    project(LoimReader VERSION ${LOIM_NUMERIC_VERSION} LANGUAGES C)

    include(GNUInstallDirs)
    include(CTest)

    option(LOIM_BUILD_CLI "Build the batch import planning CLI" ON)
    option(LOIM_BUILD_DESKTOP "Build the SDL3 desktop application" ON)
    option(LOIM_INSTALL_DEVELOPMENT "Install loim_core headers and library" OFF)
    option(LOIM_ENABLE_STRICT_WARNINGS "Enable strict compiler warnings" ON)

    function(loim_enable_strict_warnings target)
        if(NOT LOIM_ENABLE_STRICT_WARNINGS)
            return()
        endif()
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /WX)
        else()
            target_compile_options(${target} PRIVATE
                -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
            )
        endif()
    endfunction()

    add_library(loim_core
        src/core/auth.c
        src/core/document.c
        src/core/image_probe.c
        src/core/import_queue.c
        src/core/i18n.c
        src/core/layout.c
        src/core/machine_code.c
        src/core/pdf.c
        src/core/presentation.c
        src/core/seam.c
        src/core/status.c
        src/core/texture_plan.c
    )
    add_library(LoimReader::Core ALIAS loim_core)

    target_compile_features(loim_core PUBLIC c_std_17)
    target_include_directories(loim_core
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    )
    set_target_properties(loim_core PROPERTIES
        C_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )

    loim_enable_strict_warnings(loim_core)

    if(LOIM_BUILD_CLI)
        add_executable(loimreader-plan src/cli/main.c)
        target_compile_features(loimreader-plan PRIVATE c_std_17)
        target_link_libraries(loimreader-plan PRIVATE LoimReader::Core)
        set_target_properties(loimreader-plan PROPERTIES C_EXTENSIONS OFF)
        if(MINGW)
            target_link_options(loimreader-plan PRIVATE -municode)
        endif()
        loim_enable_strict_warnings(loimreader-plan)
        install(TARGETS loimreader-plan
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT CommandLine)

        if(BUILD_TESTING)
            set(LOIM_UTF8_TEST_IMAGE
                "${CMAKE_CURRENT_BINARY_DIR}/loimreader-分页测试.png")
            add_test(
                NAME loim_cli_prepare_utf8_path
                COMMAND ${CMAKE_COMMAND} -E copy
                    "${CMAKE_CURRENT_SOURCE_DIR}/images/sitelogo.png"
                    "${LOIM_UTF8_TEST_IMAGE}")
            set_tests_properties(loim_cli_prepare_utf8_path PROPERTIES
                FIXTURES_SETUP loim_utf8_fixture)
            add_test(
                NAME loim_cli_utf8_path
                COMMAND $<TARGET_FILE:loimreader-plan> "${LOIM_UTF8_TEST_IMAGE}")
            set_tests_properties(loim_cli_utf8_path PROPERTIES
                FIXTURES_REQUIRED loim_utf8_fixture)
        endif()
    endif()

    if(LOIM_BUILD_DESKTOP)
        include(FetchContent)

        # SDL and SDL_image are linked statically so each release is self-contained.
        # Commit hashes, rather than moving branch names, keep all six builds identical.
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
        set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
        set(SDL_GPU OFF CACHE BOOL "" FORCE)
        set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
        set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
        set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
        set(SDL_POWER OFF CACHE BOOL "" FORCE)
        set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
        set(SDL_TRAY OFF CACHE BOOL "" FORCE)
        set(SDL_RENDER_GPU OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDL_INSTALL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
        FetchContent_Declare(
            SDL3
            URL https://github.com/libsdl-org/SDL/archive/8e37db5e797b6167f3a00d697d816a684bd259c7.tar.gz
            URL_HASH SHA256=85aa3f7b01e91d9a0b2e8079b065c594e579c43d53d5671c8f68b20074cc896e
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(SDL3)

        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_INSTALL OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_VENDORED OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
        set(SDLIMAGE_BACKEND_IMAGEIO OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_BACKEND_WIC OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_ANI OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_BMP ON CACHE BOOL "" FORCE)
        set(SDLIMAGE_GIF ON CACHE BOOL "" FORCE)
        set(SDLIMAGE_JPG ON CACHE BOOL "" FORCE)
        set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_LBM OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_PCX OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_PNG ON CACHE BOOL "" FORCE)
        set(SDLIMAGE_PNG_LIBPNG OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_PNM OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_QOI OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_SVG OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_TGA OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_XCF OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_XPM OFF CACHE BOOL "" FORCE)
        set(SDLIMAGE_XV OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            SDL3_image
            URL https://github.com/libsdl-org/SDL_image/archive/bec9134a26c7d0f31b36d6083c25296e04cabff5.tar.gz
            URL_HASH SHA256=88f2d89e7ad04a5d5a5e5342874ca5d553be8a87a17ae4884bfa642d54f4277e
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(SDL3_image)

        set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)
        set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
        set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
        set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
        set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
        find_package(Git REQUIRED)
        FetchContent_Declare(
            SDL3_ttf
            GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
            GIT_TAG a1ce3670aec736ecbf0936c43f2f0cc53aa61e5b
            GIT_SHALLOW FALSE
            GIT_SUBMODULES external/freetype
            GIT_SUBMODULES_RECURSE TRUE
            PATCH_COMMAND
                "${CMAKE_COMMAND}"
                "-DSOURCE_DIR=<SOURCE_DIR>"
                "-DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/sdl3_ttf-null-glyph-buffer.patch"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApplySDL3TtfPatch.cmake"
        )
        FetchContent_MakeAvailable(SDL3_ttf)

        if(WIN32)
            set(LOIM_HTTP_SOURCE src/desktop/http_winhttp.c)
            add_executable(loimreader WIN32
                src/desktop/main.c ${LOIM_HTTP_SOURCE} app.rc)
        elseif(APPLE)
            find_package(CURL REQUIRED)
            set(LOIM_HTTP_SOURCE src/desktop/http_curl.c)
            add_executable(loimreader MACOSX_BUNDLE
                src/desktop/main.c ${LOIM_HTTP_SOURCE} icons/macos.icns)
            set_source_files_properties(icons/macos.icns PROPERTIES
                MACOSX_PACKAGE_LOCATION Resources)
            set_target_properties(loimreader PROPERTIES
                MACOSX_BUNDLE_BUNDLE_NAME "LoimReader"
                MACOSX_BUNDLE_BUNDLE_VERSION "${LOIM_VERSION_OVERRIDE}"
                MACOSX_BUNDLE_SHORT_VERSION_STRING "${LOIM_VERSION_OVERRIDE}"
                MACOSX_BUNDLE_GUI_IDENTIFIER "com.ctdy123.loimreader"
                MACOSX_BUNDLE_ICON_FILE "macos.icns"
            )
        else()
            find_package(CURL REQUIRED)
            set(LOIM_HTTP_SOURCE src/desktop/http_curl.c)
            add_executable(loimreader src/desktop/main.c ${LOIM_HTTP_SOURCE})
        endif()
        target_compile_features(loimreader PRIVATE c_std_17)
        target_compile_definitions(loimreader PRIVATE
            LOIM_APP_VERSION="${LOIM_VERSION_OVERRIDE}")
        target_link_libraries(loimreader PRIVATE
            LoimReader::Core
            SDL3::SDL3
            SDL3_image::SDL3_image
            SDL3_ttf::SDL3_ttf
        )
        if(APPLE)
            target_link_libraries(loimreader PRIVATE
                CURL::libcurl
                "-framework CoreFoundation"
                "-framework IOKit")
        elseif(WIN32)
            target_link_libraries(loimreader PRIVATE advapi32 winhttp)
        else()
            target_link_libraries(loimreader PRIVATE CURL::libcurl)
        endif()
        set_target_properties(loimreader PROPERTIES
            C_EXTENSIONS OFF
            OUTPUT_NAME "LoimReader"
        )
        loim_enable_strict_warnings(loimreader)

        if(BUILD_TESTING)
            add_test(
                NAME loim_desktop_smoke
                COMMAND $<TARGET_FILE:loimreader>
                    --smoke-test
                    "${CMAKE_CURRENT_SOURCE_DIR}/images/sitelogo.png"
                    "${CMAKE_CURRENT_SOURCE_DIR}/images/myshop.jpeg")
        endif()

        install(TARGETS loimreader
            BUNDLE DESTINATION . COMPONENT Runtime
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime)
        if(APPLE)
            set(LOIM_LICENSE_INSTALL_DIR
                "LoimReader.app/Contents/Resources/licenses/LoimReader")
            install(FILES
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/macOS 安装说明.txt"
                DESTINATION .
                COMPONENT Runtime)
        else()
            set(LOIM_LICENSE_INSTALL_DIR
                "${CMAKE_INSTALL_DATADIR}/licenses/LoimReader")
            # Linux desktop integration: menu and desktop launchers come from
            # a freedesktop.org .desktop file plus an icon. CPACK_CREATE_DESKTOP_LINKS
            # is a no-op for the DEB generator, so install them explicitly.
            install(FILES
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/LoimReader.desktop"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
                COMPONENT Runtime)
            install(FILES
                "${CMAKE_CURRENT_SOURCE_DIR}/icons/loimreader.png"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
                COMPONENT Runtime)
        endif()
        install(FILES
            "${sdl3_SOURCE_DIR}/LICENSE.txt"
            DESTINATION "${LOIM_LICENSE_INSTALL_DIR}"
            RENAME SDL3-LICENSE.txt
            COMPONENT Runtime)
        install(FILES
            "${sdl3_image_SOURCE_DIR}/LICENSE.txt"
            DESTINATION "${LOIM_LICENSE_INSTALL_DIR}"
            RENAME SDL3_image-LICENSE.txt
            COMPONENT Runtime)
        install(FILES
            "${sdl3_ttf_SOURCE_DIR}/LICENSE.txt"
            DESTINATION "${LOIM_LICENSE_INSTALL_DIR}"
            RENAME SDL3_ttf-LICENSE.txt
            COMPONENT Runtime)
        install(FILES
            "${sdl3_ttf_SOURCE_DIR}/external/freetype/docs/FTL.TXT"
            DESTINATION "${LOIM_LICENSE_INSTALL_DIR}"
            RENAME FreeType-LICENSE.txt
            COMPONENT Runtime)
        install(FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
            DESTINATION "${LOIM_LICENSE_INSTALL_DIR}"
            COMPONENT Runtime)
        if(APPLE)
            # Sign the installed bundle only after every resource has been
            # copied into it. Signing the build-tree bundle earlier leaves the
            # final DMG with an invalid resource seal.
            install(CODE [[
                set(_loim_app "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/LoimReader.app")
                execute_process(
                    COMMAND /usr/bin/codesign --force --deep --sign - "${_loim_app}"
                    RESULT_VARIABLE _loim_sign_result
                    ERROR_VARIABLE _loim_sign_error)
                if(NOT _loim_sign_result EQUAL 0)
                    message(FATAL_ERROR
                        "Failed to ad-hoc sign ${_loim_app}: ${_loim_sign_error}")
                endif()
                execute_process(
                    COMMAND /usr/bin/codesign --verify --deep --strict --verbose=2
                        "${_loim_app}"
                    RESULT_VARIABLE _loim_verify_result
                    ERROR_VARIABLE _loim_verify_error)
                if(NOT _loim_verify_result EQUAL 0)
                    message(FATAL_ERROR
                        "Installed macOS bundle failed signature verification: "
                        "${_loim_verify_error}")
                endif()
            ]] COMPONENT Runtime)
        endif()
    endif()

    if(BUILD_TESTING)
        add_executable(loim_core_tests tests/test_core.c)
        target_compile_features(loim_core_tests PRIVATE c_std_17)
        target_link_libraries(loim_core_tests PRIVATE LoimReader::Core)
        target_compile_definitions(loim_core_tests PRIVATE
            LOIM_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        set_target_properties(loim_core_tests PROPERTIES C_EXTENSIONS OFF)
        loim_enable_strict_warnings(loim_core_tests)
        add_test(NAME loim_core_tests COMMAND loim_core_tests)

        add_executable(loim_desktop_model_tests tests/test_desktop_model.c)
        target_compile_features(loim_desktop_model_tests PRIVATE c_std_17)
        target_link_libraries(loim_desktop_model_tests PRIVATE LoimReader::Core)
        set_target_properties(loim_desktop_model_tests PROPERTIES C_EXTENSIONS OFF)
        loim_enable_strict_warnings(loim_desktop_model_tests)
        add_test(NAME loim_desktop_model_tests COMMAND loim_desktop_model_tests)
    endif()

    if(LOIM_INSTALL_DEVELOPMENT)
        install(TARGETS loim_core
            EXPORT LoimReaderTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
        install(DIRECTORY include/loim/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/loim)
    endif()

    set(LOIM_RELEASE_PLATFORM "" CACHE STRING
        "Canonical release platform (windows, linux, or macos)")
    set(LOIM_RELEASE_ARCH "" CACHE STRING
        "Canonical release architecture (amd64 or arm64)")
    if(NOT LOIM_RELEASE_PLATFORM)
        if(WIN32)
            set(LOIM_RELEASE_PLATFORM "windows")
        elseif(APPLE)
            set(LOIM_RELEASE_PLATFORM "macos")
        else()
            set(LOIM_RELEASE_PLATFORM "linux")
        endif()
    endif()
    if(NOT LOIM_RELEASE_ARCH)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
            set(LOIM_RELEASE_ARCH "arm64")
        else()
            set(LOIM_RELEASE_ARCH "amd64")
        endif()
    endif()
    if(NOT LOIM_RELEASE_PLATFORM MATCHES "^(windows|linux|macos)$")
        message(FATAL_ERROR "LOIM_RELEASE_PLATFORM is invalid: ${LOIM_RELEASE_PLATFORM}")
    endif()
    if(NOT LOIM_RELEASE_ARCH MATCHES "^(amd64|arm64)$")
        message(FATAL_ERROR "LOIM_RELEASE_ARCH is invalid: ${LOIM_RELEASE_ARCH}")
    endif()

    set(CPACK_PACKAGE_NAME "LoimReader")
    set(CPACK_PACKAGE_VENDOR "ctdy123.com")
    set(CPACK_PACKAGE_CONTACT "ctdy123.com")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "LoimReader multi-image pagination desktop application")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://ctdy123.com/download/loimreader")
    set(CPACK_PACKAGE_VERSION ${LOIM_VERSION_OVERRIDE})
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "LoimReader")
    set(CPACK_PACKAGE_EXECUTABLES "LoimReader" "LoimReader")
    set(CPACK_CREATE_DESKTOP_LINKS "LoimReader")
    set(CPACK_COMPONENTS_ALL Runtime)
    set(CPACK_PACKAGE_FILE_NAME
        "LoimReader_${LOIM_VERSION_OVERRIDE}_${LOIM_RELEASE_PLATFORM}_${LOIM_RELEASE_ARCH}")

    if(WIN32)
        set(CPACK_GENERATOR "WIX")
        set(CPACK_WIX_VERSION "4")
        set(CPACK_WIX_UPGRADE_GUID "92464991-FFF2-44F3-82D9-5F5E84B3AD58")
        set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/icons/windows.ico")
        set(CPACK_WIX_INSTALL_SCOPE "perMachine")
        if(LOIM_RELEASE_ARCH STREQUAL "arm64")
            set(CPACK_WIX_ARCHITECTURE "arm64")
        else()
            set(CPACK_WIX_ARCHITECTURE "x64")
        endif()
    elseif(APPLE)
        set(CPACK_GENERATOR "DragNDrop")
        set(CPACK_DMG_VOLUME_NAME "LoimReader ${LOIM_VERSION_OVERRIDE}")
        set(CPACK_DMG_FORMAT "UDZO")
    else()
        set(CPACK_GENERATOR "DEB")
        set(CPACK_DEBIAN_PACKAGE_NAME "loimreader")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "ctdy123.com")
        set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
        set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${LOIM_RELEASE_ARCH}")
        set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://ctdy123.com/download/loimreader")
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "fonts-noto-cjk, cups-client")
        string(REPLACE "-" "~" LOIM_DEBIAN_VERSION "${LOIM_VERSION_OVERRIDE}")
        set(CPACK_DEBIAN_PACKAGE_VERSION "${LOIM_DEBIAN_VERSION}")
        set(CPACK_DEBIAN_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.deb")
    endif()
    include(CPack)

    return()
endif()

# 项目版本（可通过 -DPROJECT_VERSION_OVERRIDE=x.y.z 覆盖）
if(NOT DEFINED PROJECT_VERSION_OVERRIDE)
    set(PROJECT_VERSION_OVERRIDE "2.7.2")
endif()

# 项目信息
project(LoimReader 
    VERSION ${PROJECT_VERSION_OVERRIDE}
    LANGUAGES CXX
)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型（默认为Release，可通过命令行覆盖）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 在不同平台设置 Qt 查找路径
if(APPLE)
    # Homebrew 默认路径
    set(CMAKE_PREFIX_PATH "/opt/homebrew/lib")
elseif(WIN32)
    # 使用环境变量 QT_DIR（脚本会传入），例如 D:/Qt/6.8.1/mingw_64
    if(DEFINED ENV{QT_DIR})
        list(APPEND CMAKE_PREFIX_PATH $ENV{QT_DIR})
    endif()
endif()

# 查找Qt6组件
if(APPLE)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui PrintSupport Network Xml DBus)
else()
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui PrintSupport Network Xml)
endif()

# 设置Qt6相关选项
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# macOS特定设置
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0")
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    
    # 设置应用Bundle信息
    set(MACOSX_BUNDLE_BUNDLE_NAME "LoimReader")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_IDENTIFIER "com.loimreader.app")
    set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024 LoimReader. All rights reserved.")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.loimreader.app")
    
    # 设置图标
    set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/icons/macos.icns")
    set(MACOSX_BUNDLE_ICON_FILE "macos.icns")
    if(EXISTS "${APP_ICON}")
        set_source_files_properties("${APP_ICON}" 
            PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        list(APPEND PROJECT_SOURCES "${APP_ICON}")
        message(STATUS "应用图标已找到: ${APP_ICON}")
    else()
        message(WARNING "应用图标未找到: ${APP_ICON}")
    endif()
endif()

# Windows特定设置
if(WIN32)
    # 设置图标
    set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
    if(EXISTS "${APP_ICON}")
        list(APPEND PROJECT_SOURCES "${APP_ICON}")
        message(STATUS "Windows应用图标已找到: ${APP_ICON}")
    else()
        message(WARNING "Windows应用图标未找到: ${APP_ICON}")
    endif()
endif()

# 定义编译宏
add_definitions(
    -DAPP_VERSION="${PROJECT_VERSION}"
    -DFA_APP_VERSION="${PROJECT_VERSION}"
    -DFA_APP_NAME="LoimReader"
    -DQT_DEPRECATED_WARNINGS
    -DNO_POPPLER
)

# Release 模式下禁用调试输出
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(
        -DQT_NO_DEBUG_OUTPUT      # 禁用 qDebug()
        -DQT_NO_INFO_OUTPUT       # 禁用 qInfo()
        # 注意：保留 qWarning() 用于错误报告
    )
    message(STATUS "Release 模式：已禁用 qDebug 和 qInfo 输出")
endif()

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 收集源文件
set(PROJECT_SOURCES
    # 主要源文件
    main.cpp
    mainwindow.cpp
    mainwindow.h
    
    # 视图相关
    mainview.cpp
    mainview.h
    leftview.cpp
    leftview.h
    
    # 自定义Splitter（Jony Ive风格）
    customsplitter.cpp
    customsplitter.h
    customsplitterhandle.cpp
    customsplitterhandle.h
    
    # 页面和预览
    pageitem.cpp
    pageitem.h
    previewitem.cpp
    previewitem.h
    
    # 对话框
    logindialog.cpp
    logindialog.h
    aboutdialog.cpp
    aboutdialog.h
    updatedialog.cpp
    updatedialog.h
    
    # 弹出窗口
    popup.cpp
    popup.h
    
    # 工具类
    util.cpp
    util.h
    machineid.cpp
    machineid.h
    protection.cpp
    protection.h
    
    # 版本管理
    app_version.cpp
    app_version.h
    versionmanager.cpp
    versionmanager.h
    
    # 轻量级自定义更新器
    simpleupdater.cpp
    simpleupdater.h
)

# Windows资源文件
if(WIN32)
    list(APPEND PROJECT_SOURCES app.rc)
endif()

# UI文件
set(PROJECT_UI_FILES
)

# 资源文件
set(PROJECT_RESOURCES
    clps.qrc
)

# 创建可执行文件
if(APPLE)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE
        ${PROJECT_SOURCES}
        ${PROJECT_UI_FILES}
        ${PROJECT_RESOURCES}
    )
elseif(WIN32)
    # Windows GUI 应用（不显示控制台窗口）
    add_executable(${PROJECT_NAME} WIN32
        ${PROJECT_SOURCES}
        ${PROJECT_UI_FILES}
        ${PROJECT_RESOURCES}
    )
else()
    # 其他平台（Linux等）
    add_executable(${PROJECT_NAME}
        ${PROJECT_SOURCES}
        ${PROJECT_UI_FILES}
        ${PROJECT_RESOURCES}
    )
endif()

# 链接Qt6库（DBus 仅在 macOS 使用）
if(APPLE)
    target_link_libraries(${PROJECT_NAME}
        Qt6::Core Qt6::Widgets Qt6::Gui Qt6::PrintSupport Qt6::Network Qt6::Xml Qt6::DBus)
else()
    target_link_libraries(${PROJECT_NAME}
        Qt6::Core Qt6::Widgets Qt6::Gui Qt6::PrintSupport Qt6::Network Qt6::Xml)
endif()

# macOS特定链接库
if(APPLE)
    target_link_libraries(${PROJECT_NAME}
        "-framework ApplicationServices"
        "-framework AppKit"
        "-framework ImageIO"
        "-framework Metal"
        "-framework IOKit"
        "-framework DiskArbitration"
        "-framework UniformTypeIdentifiers"
        "-framework OpenGL"
        "-lcups"
    )
endif()

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "LoimReader"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
)

# 安装规则
if(APPLE)
    # 安装应用Bundle
    install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
    )
    
    # 使用macdeployqt部署Qt依赖
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS ${Qt6_DIR}/../../../bin)
        if(MACDEPLOYQT_EXECUTABLE)
            install(CODE "
                execute_process(
                    COMMAND ${MACDEPLOYQT_EXECUTABLE} \${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app -verbose=2
                    RESULT_VARIABLE deploy_result
                )
                if(deploy_result)
                    message(WARNING \"macdeployqt failed with result: \${deploy_result}\")
                endif()
            ")
        endif()
    endif()
endif()

# 构建信息输出
message(STATUS "=== 构建配置信息 ===")
message(STATUS "项目名称: ${PROJECT_NAME}")
message(STATUS "版本: ${PROJECT_VERSION}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "Qt6路径: ${Qt6_DIR}")
if(APPLE)
    message(STATUS "macOS部署目标: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
    message(STATUS "架构: ${CMAKE_OSX_ARCHITECTURES}")
    message(STATUS "Bundle标识符: ${MACOSX_BUNDLE_IDENTIFIER}")
endif()
message(STATUS "==================")

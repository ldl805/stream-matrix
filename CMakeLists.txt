cmake_minimum_required(VERSION 3.14)

set(VER_MAJ 2)
set(VER_MIN 1)
set(VER_PAT 0)
# set(REL_STATUS "alpha")

set(APP_NAME "StreamMatrix")
set(ORG_NAME "StreamMatrix")
set(ORG_DOMAIN "streammatrix.org")

set(VERSION "${VER_MAJ}.${VER_MIN}")

if(${VER_PAT} GREATER 0)
    set(VERSION "${VERSION}.${VER_PAT}")
endif()

project(stream-matrix VERSION ${VERSION} LANGUAGES C CXX)

if(DEFINED REL_STATUS)
    set(VERSION "${VERSION}-${REL_STATUS}")
endif()

### Packager and Git commit
execute_process(COMMAND git -C ${CMAKE_SOURCE_DIR} log --pretty=format:%h -n 1
    OUTPUT_VARIABLE GIT_COMMIT
    ERROR_QUIET
)

if (EXISTS "${CMAKE_SOURCE_DIR}/debian/git-build-recipe.manifest")
    set(PACKAGER "PPA")

    if ("${GIT_COMMIT}" STREQUAL "")
        file(READ "${CMAKE_SOURCE_DIR}/debian/git-build-recipe.manifest" PPA_MANIFEST)
        string(REGEX MATCH "lp:stream-matrix git-commit:(.......)" _ ${PPA_MANIFEST})
        set(GIT_COMMIT ${CMAKE_MATCH_1})
    endif()
endif()

if(NOT DEFINED PACKAGER)
    if ("${GIT_COMMIT}" STREQUAL "")
        set(PACKAGER "SRC")
    else()
        set(PACKAGER "GIT")
    endif()
endif()

if ("${GIT_COMMIT}" STREQUAL "")
    set(GIT_COMMIT "N/A")
endif()

set(VERSION "${VERSION} ${PACKAGER}.${GIT_COMMIT} (${CMAKE_SYSTEM_PROCESSOR})")
###

add_definitions(-DAPP_NAME="${APP_NAME}" -DAPP_VERSION="${VERSION}" -DORG_NAME="${ORG_NAME}" -DORG_DOMAIN="${ORG_DOMAIN}")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fix build on LP for Ubuntu 20.04
if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT ANDROID AND NOT MSVC)
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wcast-align
        -Wcast-qual
        -Wctor-dtor-privacy
        -Wduplicated-branches
        -Wduplicated-cond
        -Wextra-semi
        -Wfloat-equal
        -Wlogical-op
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wredundant-decls
    )
endif()

# Disable use of C++ API deprecated in Qt 5.15
add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=0x050F00)

set(QT_LINK_DEPENDENCIES Core Quick Multimedia)

if (ANDROID)
    list(APPEND QT_LINK_DEPENDENCIES Svg)

    set(ANDROID_EXTRA_LIBS
        "${CMAKE_SOURCE_DIR}/src/qmlav/3rd/FFmpeg/ffbuild/${ANDROID_ABI}/lib/libavformat.so"
        "${CMAKE_SOURCE_DIR}/src/qmlav/3rd/FFmpeg/ffbuild/${ANDROID_ABI}/lib/libavcodec.so"
        "${CMAKE_SOURCE_DIR}/src/qmlav/3rd/FFmpeg/ffbuild/${ANDROID_ABI}/lib/libavutil.so"
        "${CMAKE_SOURCE_DIR}/src/qmlav/3rd/FFmpeg/ffbuild/${ANDROID_ABI}/lib/libswscale.so"
        "${CMAKE_SOURCE_DIR}/src/qmlav/3rd/FFmpeg/ffbuild/${ANDROID_ABI}/lib/libswresample.so"
        "${CMAKE_SOURCE_DIR}/src/qmlav/3rd/FFmpeg/ffbuild/${ANDROID_ABI}/lib/libavdevice.so"
        "${CMAKE_SOURCE_DIR}/src/qmlav/3rd/FFmpeg/ffbuild/${ANDROID_ABI}/lib/libavfilter.so"
        )
else()
    if (WIN32)
        list(APPEND QT_LINK_DEPENDENCIES Svg)
    endif()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBAV REQUIRED
        libavformat
        libavcodec
        libavutil
        libswscale
        libswresample
        libavdevice
    )
    include_directories(${LIBAV_INCLUDE_DIRS})
    link_directories(${LIBAV_LIBRARY_DIRS})
endif()

find_package(Qt5 5.12 COMPONENTS ${QT_LINK_DEPENDENCIES} LinguistTools REQUIRED)
find_package(Qt5QuickCompiler QUIET)

file(GLOB TS_FILES "${CMAKE_SOURCE_DIR}/translations/*.ts")

if (Qt5QuickCompiler_FOUND AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    qtquick_compiler_add_resources(RESOURCES stream-matrix.qrc)
else()
    qt5_add_resources(RESOURCES stream-matrix.qrc)
endif()

set(PROJ_FILES
    src/main.cpp
    src/clipboard.h
    src/singleapplication.h
    src/config.cpp src/config.h
    src/context.cpp src/context.h
    src/eventfilter.h src/eventfilter.cpp
    src/viewportslayoutmodel.cpp src/viewportslayoutmodel.h
    src/viewportslayoutscollectionmodel.cpp src/viewportslayoutscollectionmodel.h
    ${RESOURCES}
    ${TS_FILES}
    )

set(QML_IMPORT_PATH "${CMAKE_SOURCE_DIR}/src/imports" CACHE STRING "Additional import path used to resolve QML modules in Qt Creator's code model")

enable_testing()
add_subdirectory(src/qmlav)

if (ANDROID)
    add_library(stream-matrix SHARED ${PROJ_FILES} ${QMLAV_FILES})

    target_include_directories(stream-matrix PRIVATE "${CMAKE_SOURCE_DIR}/src/qmlav/3rd/FFmpeg/ffbuild/${ANDROID_ABI}/include")
    target_link_directories(stream-matrix PRIVATE "${CMAKE_SOURCE_DIR}/src/qmlav/3rd/FFmpeg/ffbuild/${ANDROID_ABI}/lib")
elseif (WIN32)
    list(APPEND PROJ_FILES win32.rc)
    add_executable(stream-matrix WIN32 ${PROJ_FILES} ${QMLAV_FILES})
else()
    add_executable(stream-matrix ${PROJ_FILES} ${QMLAV_FILES})
endif()

target_include_directories(stream-matrix PRIVATE ${QMLAV_INCLUDE})

target_compile_definitions(stream-matrix
    PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)

foreach (ITEM ${QT_LINK_DEPENDENCIES})
    target_link_libraries(stream-matrix PRIVATE Qt5::${ITEM})
endforeach()
target_link_libraries(stream-matrix PRIVATE ${QMLAV_LINK_DEPENDENCIES})

set_directory_properties(PROPERTIES CLEAN_NO_CUSTOM true)
set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION "${CMAKE_SOURCE_DIR}/translations")
qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})

if (UNIX AND NOT ANDROID)
    include(GNUInstallDirs)
    install(TARGETS stream-matrix DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES "images/stream-matrix.svg" DESTINATION "${CMAKE_INSTALL_DATADIR}/pixmaps")
    install(FILES "images/stream-matrix.svg"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
            RENAME "org.streammatrix.StreamMatrix.svg")
    install(FILES "stream-matrix.desktop" "org.streammatrix.StreamMatrix.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "org.streammatrix.StreamMatrix.metainfo.xml" DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")

    set(CPACK_PACKAGE_NAME "stream-matrix")
    set(CPACK_PACKAGE_VENDOR "StreamMatrix")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance video matrix application for live RTSP feeds")
    set(CPACK_PACKAGE_VERSION_MAJOR ${VER_MAJ})
    set(CPACK_PACKAGE_VERSION_MINOR ${VER_MIN})
    set(CPACK_PACKAGE_VERSION_PATCH ${VER_PAT})
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${VER_MAJ}.${VER_MIN}.${VER_PAT}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "ldl805 <liedel@gmail.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "video")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    include(CPack)
elseif (WIN32)
    install(TARGETS stream-matrix RUNTIME DESTINATION .)
    install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/README.md" DESTINATION .)
    install(FILES "${CMAKE_SOURCE_DIR}/images/stream-matrix.ico" "${CMAKE_SOURCE_DIR}/images/stream-matrix.svg" DESTINATION images)

    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt-qt5 windeployqt HINTS "${_qt5Core_install_prefix}/bin")
    get_filename_component(COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

    install(CODE "
        set(EXE_PATH \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/stream-matrix.exe\")
        if(EXISTS \"\${EXE_PATH}\")
            if(EXISTS \"${WINDEPLOYQT_EXECUTABLE}\")
                message(STATUS \"Running windeployqt on \${EXE_PATH}...\")
                execute_process(
                    COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                        --release
                        --no-translations
                        --no-angle
                        --no-opengl-sw
                        --qmldir \"${CMAKE_SOURCE_DIR}/src\"
                        \"\${EXE_PATH}\"
                    RESULT_VARIABLE WINDEPLOYQT_RES
                )
            endif()

            if(POLICY CMP0207)
                cmake_policy(SET CMP0207 NEW)
            endif()
            file(GLOB DEPLOYED_DLLS \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/*.dll\")
            file(GET_RUNTIME_DEPENDENCIES
                EXECUTABLES \"\${EXE_PATH}\"
                LIBRARIES \${DEPLOYED_DLLS}
                RESOLVED_DEPENDENCIES_VAR RESOLVED_DEPS
                UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED_DEPS
                DIRECTORIES \"${COMPILER_BIN_DIR}\" ${LIBAV_LIBRARY_DIRS}
                PRE_EXCLUDE_REGEXES \"^api-ms-.*\" \"^ext-ms-.*\"
                POST_EXCLUDE_REGEXES \".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\\\\\].*\" \".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\\\\\][Ss][Yy][Ss][Ww][Oo][Ww]64[/\\\\\\\\].*\"
            )
            foreach(DEP \${RESOLVED_DEPS})
                file(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}\" TYPE SHARED_LIBRARY FILES \"\${DEP}\")
            endforeach()
        endif()
    ")

    set(PACKAGE_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
    if(PACKAGE_ARCH STREQUAL "AMD64")
        set(PACKAGE_ARCH "x86_64")
    endif()

    set(CPACK_PACKAGE_NAME "stream-matrix")
    set(CPACK_PACKAGE_VENDOR "StreamMatrix")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance video matrix application for live RTSP feeds")
    set(CPACK_PACKAGE_VERSION_MAJOR ${VER_MAJ})
    set(CPACK_PACKAGE_VERSION_MINOR ${VER_MIN})
    set(CPACK_PACKAGE_VERSION_PATCH ${VER_PAT})
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "StreamMatrix")
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${VER_MAJ}.${VER_MIN}.${VER_PAT}-${CMAKE_SYSTEM_NAME}-${PACKAGE_ARCH}")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
    set(CPACK_PACKAGE_EXECUTABLES "stream-matrix" "StreamMatrix")
    set(CPACK_CREATE_DESKTOP_LINKS "stream-matrix")

    find_program(MAKENSIS_EXECUTABLE makensis)
    if(MAKENSIS_EXECUTABLE)
        set(CPACK_GENERATOR "ZIP;NSIS")
        set(CPACK_NSIS_DISPLAY_NAME "StreamMatrix")
        set(CPACK_NSIS_PACKAGE_NAME "StreamMatrix")
        set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/images\\\\stream-matrix.ico")
        set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/images\\\\stream-matrix.ico")
        set(CPACK_NSIS_INSTALLED_ICON_NAME "stream-matrix.exe")
        set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
        set(CPACK_NSIS_MODIFY_PATH OFF)
    else()
        set(CPACK_GENERATOR "ZIP")
    endif()
    include(CPack)
endif()


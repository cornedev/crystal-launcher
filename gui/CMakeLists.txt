cmake_minimum_required(VERSION 3.19)

project(crystal LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent)
find_package(nlohmann_json REQUIRED)
find_package(LibArchive REQUIRED)
if(NOT WIN32)
    find_package(CURL REQUIRED)
endif()

qt_standard_project_setup()

set(RUNTIME_DIR ${CMAKE_SOURCE_DIR}/runtime)
set(RUNTIME_BIN_DIR ${RUNTIME_DIR}/bin)
set(PLATFORMS_DIR ${RUNTIME_DIR}/platforms)
set(GFX_DIR ${CMAKE_SOURCE_DIR}/gfx)
set(CMAKE_AUTORCC ON)

if(WIN32)
    set(ICON_RC gfx/icon.rc)
else()
    set(ICON_RC "")
endif()

qt_add_executable(crystal
    WIN32 MACOSX_BUNDLE
    main.cpp
    gui.cpp
    gui.h
    gui.ui
    resources.qrc
    ../api/crystal_vanilla.cpp
    ../api/crystal_process.cpp
    ../api/crystal_java.cpp
    ../api/crystal_fabric.cpp
    ../api/crystal_auth.cpp
    ../api/crystal_http.cpp
    ${ICON_RC}
    console.h console.cpp console.ui
)

target_include_directories(crystal PRIVATE
    ${RUNTIME_DIR}/include
)

target_link_libraries(crystal PRIVATE
    Qt::Core
    Qt::Widgets
    Qt::Concurrent
)

install(TARGETS crystal
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if (WIN32)
    set(CURL_LIB ${RUNTIME_DIR}/lib/libcurl.dll.a)
    target_link_libraries(crystal PRIVATE
        ${CURL_LIB}
        LibArchive::LibArchive
        nlohmann_json::nlohmann_json
        -lws2_32
    )
elseif(APPLE)
    target_link_libraries(crystal PRIVATE
        CURL::libcurl
        LibArchive::LibArchive
        nlohmann_json::nlohmann_json
    )
else()
    target_link_libraries(crystal PRIVATE
        CURL::libcurl
        LibArchive::LibArchive
        nlohmann_json::nlohmann_json
    )
endif()

if(WIN32)
    file(GLOB PLATFORMS "${PLATFORMS_DIR}/*.dll")
    add_custom_command(TARGET crystal POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:crystal>/platforms
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${PLATFORMS}
                $<TARGET_FILE_DIR:crystal>/platforms
    )
endif()

file(GLOB GFX_FILES CONFIGURE_DEPENDS "${GFX_DIR}/*.html")
if(GFX_FILES)
    add_custom_command(TARGET crystal POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${GFX_FILES}
                "$<TARGET_FILE_DIR:crystal>"
    )
endif()

if(WIN32)
    file(GLOB RUNTIME_DLLS "${RUNTIME_BIN_DIR}/*.dll")
    add_custom_command(TARGET crystal POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${RUNTIME_DLLS}
                $<TARGET_FILE_DIR:crystal>
        COMMENT "Copying runtime dlls to build directory..."
    )
endif()

cmake_minimum_required(VERSION 3.16)

project(QtCPP-Installer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")

set(CURL_INCLUDE_DIR "D:/GitHub/Distributed-Hash-Cracker-CPP/vcpkg/packages/curl_x64-windows/include")
set(CURL_LIBRARY "D:/GitHub/Distributed-Hash-Cracker-CPP/vcpkg/packages/curl_x64-windows/lib/libcurl.lib")

# ---- Qt ----
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Concurrent)
find_package(CURL REQUIRED)

set(TS_FILES QtCPP-Installer_en_US.ts)

# ---- Sources ----
set(SOURCES
    main.cpp
    mainwindow.cpp
    downloadmanager.cpp
    segmentdownloader.cpp
    utils.cpp
)

set(HEADERS
    mainwindow.h
    downloadmanager.h
    segmentdownloader.h
    utils.h
)

set(UI mainwindow.ui)

qt_add_executable(QtCPP-Installer
    MANUAL_FINALIZATION
    ${SOURCES}
    ${UI}
    ${TS_FILES}
)

qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})

# ---- Qt Resources ----
qt_add_resources(resources resources/resources.qrc)
target_sources(QtCPP-Installer PRIVATE ${resources})

# ---- Bit7z ----
set(BIT7Z_INCLUDE_DIR "D:/GitHub/bit7z/include")
set(BIT7Z_LIB_DIR "D:/GitHub/bit7z/lib/x64")
target_include_directories(QtCPP-Installer PRIVATE ${BIT7Z_INCLUDE_DIR})

# Correct Debug/Release handling
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(QtCPP-Installer PRIVATE "${BIT7Z_LIB_DIR}/Debug/bit7z.lib")
else()
    target_link_libraries(QtCPP-Installer PRIVATE "${BIT7Z_LIB_DIR}/Release/bit7z.lib")
endif()

# ---- Link Qt ----
target_link_libraries(QtCPP-Installer PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Threads::Threads)

# ---- Finalize ----
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(QtCPP-Installer)
endif()

target_include_directories(QtCPP-Installer PRIVATE ${CURL_INCLUDE_DIR})
target_link_libraries(QtCPP-Installer PRIVATE CURL::libcurl)

# Output directory (Qt Creator uses this for Debug/Release)
set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")

# Copy libcurl-x64 DLL
add_custom_command(TARGET QtCPP-Installer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/dependencies/libcurl-x64.dll"
        "$<TARGET_FILE_DIR:QtCPP-Installer>/libcurl-x64.dll"
    COMMENT "Copying libcurl-x64.dll to build output folder"
)

# Copy libcurl DLL
add_custom_command(TARGET QtCPP-Installer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/dependencies/libcurl.dll"
        "$<TARGET_FILE_DIR:QtCPP-Installer>/libcurl.dll"
    COMMENT "Copying libcurl.dll to build output folder"
)

# Copy 7z.dll
add_custom_command(TARGET QtCPP-Installer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Program Files/7-Zip/7z.dll"
        "$<TARGET_FILE_DIR:QtCPP-Installer>/7z.dll"
    COMMENT "Copying 7z.dll"
)

# Copy resources/data folder
add_custom_command(TARGET QtCPP-Installer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources/data"
        "$<TARGET_FILE_DIR:QtCPP-Installer>/data"
    COMMENT "Copying data folder"
)
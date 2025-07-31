cmake_minimum_required(VERSION 3.16)

project(QtCPP-Installer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    # MSVC compiler flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    # GCC/Clang compiler flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ---- Qt ----
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Concurrent)

set(TS_FILES QtCPP-Installer_en_US.ts)

# ---- Sources ----
set(SOURCES
    main.cpp
    mainwindow.cpp
    downloadmanager.cpp
)

set(HEADERS
    mainwindow.h
    downloadmanager.h
    utils.h
)

set(UI mainwindow.ui)

qt_add_executable(QtCPP-Installer WIN32
    MANUAL_FINALIZATION
    ${SOURCES}
    ${UI}
    ${TS_FILES}
)

qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})

# ---- Qt Resources ----
qt_add_resources(resources resources/resources.qrc)
target_sources(QtCPP-Installer PRIVATE ${resources})

if(WIN32)
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
endif()

if(UNIX)
    target_link_libraries(QtCPP-Installer PRIVATE Threads::Threads)
endif()

# ---- Link Qt ----
target_link_libraries(QtCPP-Installer PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Threads::Threads)

# ---- Finalize ----
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(QtCPP-Installer)
endif()

if(WIN32)
    set(CURL_INCLUDE_DIR "D:/GitHub/vcpkg/packages/curl_x64-windows/include")
    set(CURL_LIBRARY "D:/GitHub/vcpkg/packages/curl_x64-windows/lib/libcurl.lib")
    include_directories(${CURL_INCLUDE_DIR})
    target_include_directories(QtCPP-Installer PRIVATE ${CURL_INCLUDE_DIR})
    target_link_libraries(QtCPP-Installer PRIVATE "${CURL_LIBRARY}")
    message(STATUS "Using CURL from: ${CURL_INCLUDE_DIR}")
else()
    find_package(CURL REQUIRED)
    if (CURL_FOUND)
        include_directories(${CURL_INCLUDE_DIRS})
    endif()
endif()

if(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
    target_link_libraries(${PROJECT_NAME} PRIVATE bit7z)
    target_link_libraries(QtCPP-Installer PRIVATE CURL::libcurl)
endif()

# Output directory (Qt Creator uses this for Debug/Release)
set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")

if(WIN32)
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

# Copy zlib1 DLL
add_custom_command(TARGET QtCPP-Installer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/dependencies/zlib1.dll"
        "$<TARGET_FILE_DIR:QtCPP-Installer>/zlib1.dll"
    COMMENT "Copying zlib1.dll to build output folder"
)

# Copy resources/data folder
#add_custom_command(TARGET QtCPP-Installer POST_BUILD
    #COMMAND ${CMAKE_COMMAND} -E copy_directory
        #"${CMAKE_SOURCE_DIR}/resources/data"
        #"$<TARGET_FILE_DIR:QtCPP-Installer>/data"
    #COMMENT "Copying data folder"
#)
endif()

    get_target_property(QtCore_location Qt6::Core IMPORTED_LOCATION_RELEASE)
    get_filename_component(QtCore_bin_dir "${QtCore_location}" DIRECTORY)
   
if(WIN32) 
    # Create the platforms directory once
    add_custom_command(TARGET QtCPP-Installer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:QtCPP-Installer>/platforms"
    )

    # Debug config: copy qwindowsd.dll
    add_custom_command(TARGET QtCPP-Installer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QtCore_bin_dir}/../plugins/platforms/qwindowsd.dll"
            "$<TARGET_FILE_DIR:QtCPP-Installer>/platforms/qwindowsd.dll"
        COMMENT "Copying qwindowsd.dll for Debug"
        CONFIGURATIONS Debug
    )

    # Release config: copy qwindows.dll
    add_custom_command(TARGET QtCPP-Installer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QtCore_bin_dir}/../plugins/platforms/qwindows.dll"
            "$<TARGET_FILE_DIR:QtCPP-Installer>/platforms/qwindows.dll"
        COMMENT "Copying qwindows.dll for Release"
        CONFIGURATIONS Release
    )

    set(QT_DLLS
        "Qt6Widgets.dll"
        "Qt6Gui.dll"
        "Qt6Core.dll"
        "Qt6Widgetsd.dll"
        "Qt6Guid.dll"
        "Qt6Cored.dll"
    )

    foreach(dll ${QT_DLLS})
        add_custom_command(TARGET QtCPP-Installer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QtCore_bin_dir}/${dll}"
                $<TARGET_FILE_DIR:QtCPP-Installer>
        )
    endforeach()
endif()

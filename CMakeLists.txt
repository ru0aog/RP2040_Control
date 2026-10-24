
cmake_minimum_required(VERSION 3.21)

project(RP2040_Control LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "C:/Qt/6.7.3/msvc2022_64")

find_package(Qt6 6.7.3 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(RP2040_Control
    main.cpp
    mainwindow.h
    mainwindow.cpp
)

target_link_libraries(RP2040_Control PRIVATE Qt6::Widgets)

if(WIN32)
    target_link_libraries(RP2040_Control PRIVATE user32)

    set_property(TARGET RP2040_Control PROPERTY
        VS_DEBUGGER_ENVIRONMENT
        "PATH=C:/Qt/6.7.3/msvc2022_64/bin;%PATH%"
    )
endif()

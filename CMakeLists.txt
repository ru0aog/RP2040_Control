cmake_minimum_required(VERSION 3.21)

project(RP2040_Control LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED CMAKE_PREFIX_PATH)  
    set(CMAKE_PREFIX_PATH "C:/Qt/6.7.3/msvc2022_64")  
endif()  

find_package(Qt6 6.7.3 REQUIRED COMPONENTS
    Widgets
)

qt_standard_project_setup()

qt_add_executable(RP2040_Control  
    main.cpp  
    mainwindow.h  
    mainwindow.cpp  
    spectrumwidget.h  
    spectrumwidget.cpp  
    app.rc  
)

set_target_properties(RP2040_Control PROPERTIES  
    WIN32_EXECUTABLE TRUE  
)

target_link_libraries(RP2040_Control PRIVATE
    Qt6::Widgets
    winmm
)

if(WIN32)  
    target_link_libraries(RP2040_Control PRIVATE  
        user32  
        dwmapi  
    )  
  
    if(MINGW)  
        target_link_options(RP2040_Control PRIVATE  
            -static  
            -static-libgcc  
            -static-libstdc++  
        )  
    endif()  
  
    set_property(TARGET RP2040_Control PROPERTY  
        VS_DEBUGGER_ENVIRONMENT  
        "PATH=C:/Qt/6.7.3/msvc2022_64/bin;%PATH%"  
    )  
endif()

cmake_minimum_required(VERSION 3.25)

project(msimeui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(
    /D_UNICODE=1
    /DUNICODE=1
    /DNOMINMAX=1
    /DWIN32_LEAN_AND_MEAN=1
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(msimeui STATIC
    src/Application.cpp
    src/Brush.cpp
    src/DebugLog.cpp
    src/DeviceResources.cpp
    src/TextBox.cpp
    src/Layout.cpp
    src/Scene.cpp
    src/Window.cpp
    src/WindowClass.cpp
    src/tsf/DisplayAttribute.cpp
    src/tsf/TextContainer.cpp
    src/tsf/TextEditor.cpp
    src/tsf/TextEditSink.cpp
    src/tsf/TextLayout.cpp
    src/tsf/TextStore.cpp
    src/tsf/TsfTextServices.cpp
    include/msimeui/Application.h
    include/msimeui/Brush.h
    src/DebugLog.h
    include/msimeui/Controls.h
    include/msimeui/DeviceResources.h
    include/msimeui/Layout.h
    include/msimeui/Scene.h
    include/msimeui/Types.h
    include/msimeui/Window.h
    src/tsf/DisplayAttribute.h
    src/tsf/private.h
    src/tsf/TextContainer.h
    src/tsf/TextEditor.h
    src/tsf/TextEditSink.h
    src/tsf/TextLayout.h
    src/tsf/TextStore.h
    src/tsf/TsfTextServices.h
)

target_include_directories(msimeui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tsf
)

target_link_libraries(msimeui PUBLIC
    d2d1.lib
    dwrite.lib
    imm32.lib
    ole32.lib
    oleaut32.lib
    uuid.lib
)

add_executable(msimeui-demo WIN32
    demos/msimeui-demo/DemoScene.cpp
    demos/msimeui-demo/DemoScene.h
    demos/msimeui-demo/main.cpp
)

target_link_libraries(msimeui-demo PRIVATE msimeui)

if(MSVC)
    target_compile_options(msimeui PRIVATE /W4 /permissive-)
    target_compile_options(msimeui-demo PRIVATE /W4 /permissive-)
endif()

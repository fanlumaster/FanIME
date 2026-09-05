cmake_minimum_required(VERSION 3.25)

project(TsfEditControl VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(
    /D_UNICODE=1
    /DUNICODE=1
)

find_package(fmt CONFIG REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(TSF_EDIT_CONTROL_SOURCES
    "./src/DisplayAttribute.cpp"
    "./src/TextContainer.cpp"
    "./src/TextEditor.cpp"
    "./src/TextEditSink.cpp"
    "./src/TextInputCtrl.cpp"
    "./src/TextLayout.cpp"
    "./src/TextStore.cpp"
    "./src/TsfD2DTextBox.cpp"
    "./src/TsfTextServices.cpp"
)

set(TSF_EDIT_CONTROL_HEADERS
    "./include/TsfD2DTextBox.h"
    "./src/DisplayAttribute.h"
    "./src/private.h"
    "./src/TextContainer.h"
    "./src/TextEditor.h"
    "./src/TextEditSink.h"
    "./src/TextInputCtrl.h"
    "./src/TextLayout.h"
    "./src/TextStore.h"
    "./src/TsfTextServices.h"
)

add_library(TsfEditControl STATIC
    ${TSF_EDIT_CONTROL_SOURCES}
    ${TSF_EDIT_CONTROL_HEADERS}
)

add_library(TsfD2DTextBox ALIAS TsfEditControl)

if(MSVC)
    target_compile_options(TsfEditControl PRIVATE /W4 /permissive-)
endif()

target_include_directories(TsfEditControl
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(TsfEditControl PUBLIC
    d2d1.lib
    dwrite.lib
    imm32.lib
    fmt::fmt-header-only
)

add_executable(TsfEditControlDemo WIN32
    "./demo/TsfD2DTextBoxDemo.cpp"
)

target_include_directories(TsfEditControlDemo PRIVATE "./include")
target_link_libraries(TsfEditControlDemo PRIVATE TsfEditControl)

install(TARGETS TsfEditControl
    EXPORT TsfEditControlTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES "./include/TsfD2DTextBox.h" DESTINATION include)

install(EXPORT TsfEditControlTargets
    FILE TsfEditControlTargets.cmake
    NAMESPACE TsfEditControl::
    DESTINATION lib/cmake/TsfEditControl
)

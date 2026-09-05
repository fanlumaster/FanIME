cmake_minimum_required(VERSION 3.25.0)
project(MetasequoiaImeLog VERSION 1.0)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_definitions(
    /D_UNICODE=1
    /DUNICODE=1
)

# fmt
find_package(fmt REQUIRED)

set(MY_EXECUTABLE_NAME "MetasequoiaImeLog")
set(
  SOURCE_FILES
  "./src/main.cpp"
)

add_executable(${MY_EXECUTABLE_NAME} ${SOURCE_FILES})

target_link_libraries(${MY_EXECUTABLE_NAME} PRIVATE fmt::fmt)
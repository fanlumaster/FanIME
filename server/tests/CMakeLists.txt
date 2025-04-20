cmake_minimum_required(VERSION 3.25.0)
project(WinCppTemplate VERSION 1.0 LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(fmt CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

add_definitions(
    /D_UNICODE=1
    /DUNICODE=1
)

include_directories("./includes/")

set(MY_EXECUTABLE_NAME "WinCppTemplate")
set(
  SOURCE_FILES
  "./src/main.cpp"
)

add_executable(${MY_EXECUTABLE_NAME} ${SOURCE_FILES} )

target_link_libraries(${MY_EXECUTABLE_NAME} fmt::fmt unofficial::sqlite3::sqlite3)
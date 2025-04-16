cmake_minimum_required(VERSION 3.25.0)

set(PROJECT_NAME "fanimeui")

project(${PROJECT_NAME} VERSION 1.0)

# Boost
set(Boost_ROOT "C:/Users/SonnyCalcr/scoop/apps/boost/1.87.0")
set (Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS locale json)

# fmt and spdlog
set(CMAKE_PREFIX_PATH "C:/Users/SonnyCalcr/EDisk/CppCodes/Libraries/AllLibs")
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_definitions(
    /D_UNICODE=1
    /DUNICODE=1
)

# add_definitions(
#     /DFANY_DEBUG=1
# )

include_directories("./src/")

set(MY_EXECUTABLE_NAME "fanimeui")

set(SRC_FILES
    "./src/defines/defines.h"
    "./src/ipc/event_listener.cpp"
    "./src/ipc/event_listener.h"
    "./src/log/fanylog.cpp"
    "./src/log/fanylog.h"
    "./src/utils/webview_utils.cpp"
    "./src/utils/webview_utils.h"
    "./src/utils/window_utils.cpp"
    "./src/utils/window_utils.h"
    "./src/utils/common_utils.cpp"
    "./src/utils/common_utils.h"
    "./src/webview2/candidate_window_webview2.cpp"
    "./src/webview2/candidate_window_webview2.h"
    "./src/window/candidate_window.cpp"
    "./src/window/candidate_window.h"
    "./src/main.cpp"
)

add_executable(${MY_EXECUTABLE_NAME} WIN32
    ${SRC_FILES}
)

# WebView2
set_target_properties(${PROJECT_NAME} PROPERTIES VS_GLOBAL_WebView2LoaderPreference "Static")
set_target_properties(${PROJECT_NAME} PROPERTIES VS_USER_PROPS "${CMAKE_SOURCE_DIR}/FanImeUi.props")
set_target_properties(${PROJECT_NAME} PROPERTIES VS_PACKAGE_REFERENCES "Microsoft.Web.WebView2_1.0.3124.44;Microsoft.Windows.ImplementationLibrary_1.0.240803.1")

target_link_libraries(${PROJECT_NAME} fmt::fmt spdlog::spdlog Boost::locale Boost::json)
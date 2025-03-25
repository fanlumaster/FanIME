cmake_minimum_required(VERSION 3.25.0)

set(PROJECT_NAME "fanimeui")

project(${PROJECT_NAME} VERSION 1.0)

add_subdirectory(fmt)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)


add_definitions(
    /D_UNICODE=1
    /DUNICODE=1
)

# add_definitions(
#     /DFANY_DEBUG=1
# )

include_directories("./includes/")

set(HEADER_FILES
    ./includes/main.h
)

set(SRC_FILES
    "src/webview2/MainWindow.cpp"
)

add_executable(fanimeui WIN32
    ${HEADER_FILES} 
    ${SRC_FILES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_GLOBAL_WebView2LoaderPreference "Static")
set_target_properties(${PROJECT_NAME} PROPERTIES VS_USER_PROPS "${CMAKE_SOURCE_DIR}/FanIME_UI.props")
set_target_properties(${PROJECT_NAME} PROPERTIES VS_PACKAGE_REFERENCES "Microsoft.Web.WebView2_1.0.3124.44;Microsoft.Windows.ImplementationLibrary_1.0.240803.1")

target_link_libraries(${PROJECT_NAME} fmt::fmt)
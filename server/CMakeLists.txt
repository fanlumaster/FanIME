cmake_minimum_required(VERSION 3.25.0)

set(PROJECT_NAME "MetasequoiaImeServer")

cmake_policy(SET CMP0167 NEW)
project(${PROJECT_NAME} VERSION 1.0)

include(CTest)

# Boost
set(Boost_ROOT "C:/Users/sonnycalcr/scoop/apps/boost/current")
set (Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS locale json)
include_directories(${Boost_INCLUDE_DIRS})

# fmt, spdlog and sqlite3
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_definitions(
    /D_UNICODE=1
    /DUNICODE=1
    /DFANY_DEBUG=1
)

# add_definitions(
#     /DFANY_DEBUG=1
#     /DFANY_IPC_DEBUG=1
# )

# For utf8cpp, make it use C++11 and later
add_compile_options(/Zc:__cplusplus)

include_directories("./")
include_directories("./src/")
include_directories("./MetasequoiaImeEngine/utfcpp/source/")

set(MY_EXECUTABLE_NAME "MetasequoiaImeServer")

set(SRC_FILES
    "./src/ai/ai_assistant.cpp"
    "./src/ai/ai_assistant.h"
    "./src/cloud/cloud_ime.cpp"
    "./src/cloud/cloud_ime.h"
    "./src/config/ime_config.cpp"
    "./src/config/ime_config.h"
    "./src/defines/defines.h"
    "./src/english/english_ime.cpp"
    "./src/english/english_ime.h"
    "./src/global/globals.h"
    "./src/ipc/event_listener.cpp"
    "./src/ipc/event_listener.h"
    "./src/ipc/ipc.cpp"
    "./src/ipc/ipc.h"
    "./src/log/fanylog.cpp"
    "./src/log/fanylog.h"
    "./src/quick-phrases/quick_phrase_query.cpp"
    "./src/quick-phrases/quick_phrase_query.h"
    "./src/resource/resource.h"
    "./src/resource/resource.rc"
    "./src/settings/settings_launcher.cpp"
    "./src/settings/settings_launcher.h"
    "./src/session/engine_input_session.cpp"
    "./src/session/engine_input_session.h"
    "./src/session/input_session.h"
    "./src/session/session_factory.cpp"
    "./src/session/session_factory.h"
    "./src/session/shuangpin_input_session.cpp"
    "./src/session/shuangpin_input_session.h"
    "./src/utils/common_utils.cpp"
    "./src/utils/common_utils.h"
    "./src/utils/ime_utils.cpp"
    "./src/utils/ime_utils.h"
    "./src/utils/webview_utils.cpp"
    "./src/utils/webview_utils.h"
    "./src/utils/window_utils.cpp"
    "./src/utils/window_utils.h"
    "./src/webview2/windows_webview2.cpp"
    "./src/webview2/windows_webview2.h"
    "./src/window/ime_windows.cpp"
    "./src/window/ime_windows.h"
    "./src/window/window_hook.cpp"
    "./src/window/window_hook.h"
    "./src/voice-input/voice_input_service.cpp"
    "./src/voice-input/voice_input_service.h"
    "./src/voice-input/wave_overlay.cpp"
    "./src/voice-input/wave_overlay.h"
    "./src/voice-input/cue_player.cpp"
    "./src/voice-input/cue_player.h"
    "./src/voice-input/mvi_utils.h"
    "./src/voice-input/voice_input_overlay_utils.cpp"
    "./src/main.cpp"
)

set(SRC_FILES_IME_ENGINE
    "./MetasequoiaImeEngine/common/helpcode_utils.cpp"
    "./MetasequoiaImeEngine/common/string_utils.cpp"
    "./MetasequoiaImeEngine/core/ime_session.cpp"
    "./MetasequoiaImeEngine/english/english_dictionary.cpp"
    "./MetasequoiaImeEngine/english/english_dictionary.h"
    "./MetasequoiaImeEngine/providers/provider_registry.cpp"
    "./MetasequoiaImeEngine/providers/pinyin_candidate_provider.cpp"
    "./MetasequoiaImeEngine/providers/wubi_candidate_provider.cpp"
    "./MetasequoiaImeEngine/quanpin/engine.cpp"
    "./MetasequoiaImeEngine/quanpin/quanpin_dictionary.cpp"
    "./MetasequoiaImeEngine/quanpin/quanpin_query.cpp"
    "./MetasequoiaImeEngine/quanpin/quanpin_utils.cpp"
    "./MetasequoiaImeEngine/schemes/quanpin_scheme.cpp"
    "./MetasequoiaImeEngine/schemes/shuangpin_scheme.cpp"
    "./MetasequoiaImeEngine/schemes/wubi_scheme.cpp"
    "./MetasequoiaImeEngine/shuangpin/engine.cpp"
    "./MetasequoiaImeEngine/shuangpin/shuangpin_dictionary.cpp"
    "./MetasequoiaImeEngine/shuangpin/shuangpin_profile.cpp"
    "./MetasequoiaImeEngine/shuangpin/shuangpin_query.cpp"
    "./MetasequoiaImeEngine/shuangpin/shuangpin_utils.cpp"
)

set(SRC_FILES_GOOGLEIME
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/dictbuilder.cpp"
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/dictlist.cpp"
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/dicttrie.cpp"
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/lpicache.cpp"
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/matrixsearch.cpp"
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/mystdlib.cpp"
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/ngram.cpp"
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/pinyinime.cpp"
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/searchutility.cpp"
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/spellingtable.cpp"
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/spellingtrie.cpp"
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/splparser.cpp"
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/sync.cpp"
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/userdict.cpp"
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/utf16char.cpp"
    "./MetasequoiaImeEngine/googlepinyinime-rev/src/share/utf16reader.cpp"
)

add_executable(${MY_EXECUTABLE_NAME} WIN32
    ${SRC_FILES}
    ${SRC_FILES_IME_ENGINE}
    ${SRC_FILES_GOOGLEIME}
)

# Tomlplusplus, Webview2 and other head-only libraries
target_include_directories(${PROJECT_NAME}
    PRIVATE
    "${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x64-windows/include"
    "${CMAKE_SOURCE_DIR}/src/voice-input"
    "${CMAKE_SOURCE_DIR}/src/voice-input/third_party"
)

target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::sqlite3::sqlite3 fmt::fmt spdlog::spdlog Boost::locale Boost::json nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME} PRIVATE winhttp)
target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
target_link_libraries(${PROJECT_NAME} PRIVATE d2d1 dwmapi shell32)

# Keep msimeui as an independently versioned GUI framework submodule. The
# product-owned emoji panel lives under src/emoji-panel so it can evolve separately
# from the framework's example applications.
add_subdirectory(msimeui EXCLUDE_FROM_ALL)
set(EMOJI_EXECUTABLE_NAME "MetasequoiaImeEmojiPanel")
add_executable(${EMOJI_EXECUTABLE_NAME} WIN32
    "./src/emoji-panel/EmojiPanel.cpp"
    "./src/emoji-panel/EmojiPanel.h"
    "./src/emoji-panel/main.cpp"
    "./src/emoji-panel/emoji_panel.rc"
    "./src/emoji-panel/resource.h"
)
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX=1)
target_link_libraries(${EMOJI_EXECUTABLE_NAME} PRIVATE msimeui)
target_compile_definitions(${EMOJI_EXECUTABLE_NAME} PRIVATE
    NOMINMAX=1
    WIN32_LEAN_AND_MEAN=1
)
if(MSVC)
    target_compile_options(${EMOJI_EXECUTABLE_NAME} PRIVATE /W4 /permissive-)
endif()
add_dependencies(${PROJECT_NAME} ${EMOJI_EXECUTABLE_NAME})

set(KEYBOARD_EXECUTABLE_NAME "MetasequoiaImeKeyboardPanel")
add_executable(${KEYBOARD_EXECUTABLE_NAME} WIN32
    "./src/keyboard-panel/KeyboardPanel.cpp"
    "./src/keyboard-panel/KeyboardPanel.h"
    "./src/keyboard-panel/main.cpp"
    "./src/keyboard-panel/keyboard_panel.rc"
    "./src/keyboard-panel/resource.h"
)
target_link_libraries(${KEYBOARD_EXECUTABLE_NAME} PRIVATE msimeui)
target_compile_definitions(${KEYBOARD_EXECUTABLE_NAME} PRIVATE
    NOMINMAX=1
    WIN32_LEAN_AND_MEAN=1
)
if(MSVC)
    target_compile_options(${KEYBOARD_EXECUTABLE_NAME} PRIVATE /W4 /permissive-)
endif()
add_dependencies(${PROJECT_NAME} ${KEYBOARD_EXECUTABLE_NAME})

# WebView2 loader (required for CreateCoreWebView2EnvironmentWithOptions)
set(WEBVIEW2_VCPKG_ROOT "${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x64-windows")
set(WEBVIEW2_LOADER_RELEASE "${WEBVIEW2_VCPKG_ROOT}/lib/WebView2Loader.dll.lib")
set(WEBVIEW2_LOADER_DEBUG "${WEBVIEW2_VCPKG_ROOT}/debug/lib/WebView2Loader.dll.lib")
target_link_libraries(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:${WEBVIEW2_LOADER_DEBUG}>
    $<$<CONFIG:Release>:${WEBVIEW2_LOADER_RELEASE}>
)

set(SETTINGS_EXECUTABLE_NAME "MetasequoiaImeSettings")
add_executable(${SETTINGS_EXECUTABLE_NAME} WIN32
    "./src/settings/settings_app.cpp"
    "./src/settings/settings_launcher.cpp"
    "./src/settings/settings_launcher.h"
    "./src/settings/dictionary_manager.cpp"
    "./src/settings/dictionary_manager.h"
    "./src/config/ime_config.cpp"
    "./src/config/ime_config.h"
    "./src/utils/common_utils.cpp"
    "./src/utils/common_utils.h"
    "./MetasequoiaImeEngine/common/helpcode_utils.cpp"
    "./MetasequoiaImeEngine/quanpin/quanpin_utils.cpp"
    "./MetasequoiaImeEngine/quanpin/quanpin_query.cpp"
    "./MetasequoiaImeEngine/shuangpin/shuangpin_profile.cpp"
    "./MetasequoiaImeEngine/shuangpin/shuangpin_utils.cpp"
    "./src/resource/resource.rc"
)
target_include_directories(${SETTINGS_EXECUTABLE_NAME}
    PRIVATE
    "${CMAKE_SOURCE_DIR}/build/vcpkg_installed/x64-windows/include"
)
target_link_libraries(${SETTINGS_EXECUTABLE_NAME} PRIVATE
    fmt::fmt
    Boost::locale
    Boost::json
    nlohmann_json::nlohmann_json
    unofficial::sqlite3::sqlite3
    spdlog::spdlog
    dwmapi
    dcomp
    shell32
    $<$<CONFIG:Debug>:${WEBVIEW2_LOADER_DEBUG}>
    $<$<CONFIG:Release>:${WEBVIEW2_LOADER_RELEASE}>
)
# The server launches the settings executable from the same output directory.
# Building the server target alone must therefore also produce Settings.
add_dependencies(${PROJECT_NAME} ${SETTINGS_EXECUTABLE_NAME})
add_custom_command(TARGET ${SETTINGS_EXECUTABLE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<$<CONFIG:Debug>:${WEBVIEW2_VCPKG_ROOT}/debug/bin/WebView2Loader.dll>
        $<$<CONFIG:Release>:${WEBVIEW2_VCPKG_ROOT}/bin/WebView2Loader.dll>
        $<TARGET_FILE_DIR:${SETTINGS_EXECUTABLE_NAME}>
)

# Ensure the loader DLL is next to the executable
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<$<CONFIG:Debug>:${WEBVIEW2_VCPKG_ROOT}/debug/bin/WebView2Loader.dll>
        $<$<CONFIG:Release>:${WEBVIEW2_VCPKG_ROOT}/bin/WebView2Loader.dll>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/audios
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/audios/start.mp3
        ${CMAKE_SOURCE_DIR}/assets/audios/end.mp3
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/audios
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

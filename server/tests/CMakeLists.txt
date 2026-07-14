set(TEST_EXECUTABLE_NAME "MetasequoiaImeServerTests")

set(TEST_SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_shuangpin_scheme.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_quanpin_scheme.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_wubi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_shuangpin_query.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_engine_shuangpin_session.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/test_english_dictionary.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/english/english_dictionary.cpp"
    "${CMAKE_SOURCE_DIR}/src/config/ime_config.cpp"
    "${CMAKE_SOURCE_DIR}/src/session/engine_input_session.cpp"
    "${CMAKE_SOURCE_DIR}/src/utils/common_utils.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/common/helpcode_utils.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/core/ime_session.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/common/string_utils.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/providers/provider_registry.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/providers/pinyin_candidate_provider.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/providers/wubi_candidate_provider.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/quanpin/engine.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/quanpin/quanpin_dictionary.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/quanpin/quanpin_utils.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/schemes/shuangpin_scheme.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/schemes/quanpin_scheme.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/schemes/wubi_scheme.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/quanpin/quanpin_query.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/shuangpin/engine.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/shuangpin/shuangpin_dictionary.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/shuangpin/shuangpin_profile.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/shuangpin/shuangpin_query.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/shuangpin/shuangpin_utils.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/dictbuilder.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/dictlist.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/dicttrie.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/lpicache.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/matrixsearch.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/mystdlib.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/ngram.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/pinyinime.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/searchutility.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/spellingtable.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/spellingtrie.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/splparser.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/sync.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/userdict.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/utf16char.cpp"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/googlepinyinime-rev/src/share/utf16reader.cpp"
)

add_executable(${TEST_EXECUTABLE_NAME} ${TEST_SOURCE_FILES})

target_include_directories(${TEST_EXECUTABLE_NAME}
    PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/MetasequoiaImeEngine/utfcpp/source"
)

target_link_libraries(${TEST_EXECUTABLE_NAME}
    PRIVATE
    fmt::fmt
    spdlog::spdlog
    Boost::locale
    Boost::json
    unofficial::sqlite3::sqlite3
)

add_test(NAME ${TEST_EXECUTABLE_NAME} COMMAND ${TEST_EXECUTABLE_NAME})

cmake_minimum_required(VERSION 3.25.0)
project(WinCppTemplate VERSION 1.0 LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Boost
set(Boost_ROOT "C:/Users/sonnycalcr/scoop/apps/boost/current")
set (Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS locale json)
include_directories(${Boost_INCLUDE_DIRS})

# fmt, spdlog and sqlite3
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

add_definitions(
    /D_UNICODE=1
    /DUNICODE=1
)

include_directories("./includes/")
include_directories("../src/")
include_directories("../")
include_directories("../MetasequoiaImeEngine/utfcpp/source/")

set(MY_EXECUTABLE_NAME "imetest")
set(
    SOURCE_FILES
    "./src/test_shuangpin.cpp"
    "../MetasequoiaImeEngine/shuangpin/dictionary.cpp"
    "../MetasequoiaImeEngine/shuangpin/pinyin_utils.cpp"
    # Google IME
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/dictbuilder.cpp"
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/dictlist.cpp"
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/dicttrie.cpp"
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/lpicache.cpp"
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/matrixsearch.cpp"
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/mystdlib.cpp"
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/ngram.cpp"
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/pinyinime.cpp"
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/searchutility.cpp"
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/spellingtable.cpp"
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/spellingtrie.cpp"
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/splparser.cpp"
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/sync.cpp"
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/userdict.cpp"
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/utf16char.cpp"
    "../MetasequoiaImeEngine/googlepinyinime-rev/src/share/utf16reader.cpp"
)

add_executable(${MY_EXECUTABLE_NAME} ${SOURCE_FILES} )

target_link_libraries(${MY_EXECUTABLE_NAME} fmt::fmt spdlog::spdlog unofficial::sqlite3::sqlite3 Boost::locale)
cmake_minimum_required(VERSION 3.25.0)
project(WinCppTemplate VERSION 1.0 LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Boost
set(Boost_ROOT "C:/Users/SonnyCalcr/scoop/apps/boost/1.87.0")
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

set(MY_EXECUTABLE_NAME "imetest")
set(
    SOURCE_FILES
    "./src/test_shuangpin.cpp"
    "../src/ime_engine/shuangpin/dictionary.cpp"
    "../src/ime_engine/shuangpin/pinyin_utils.cpp"
    # Google IME
    "../src/ime_engine/googlepinyinime-rev/src/share/dictbuilder.cpp"
    "../src/ime_engine/googlepinyinime-rev/src/share/dictlist.cpp"
    "../src/ime_engine/googlepinyinime-rev/src/share/dicttrie.cpp"
    "../src/ime_engine/googlepinyinime-rev/src/share/lpicache.cpp"
    "../src/ime_engine/googlepinyinime-rev/src/share/matrixsearch.cpp"
    "../src/ime_engine/googlepinyinime-rev/src/share/mystdlib.cpp"
    "../src/ime_engine/googlepinyinime-rev/src/share/ngram.cpp"
    "../src/ime_engine/googlepinyinime-rev/src/share/pinyinime.cpp"
    "../src/ime_engine/googlepinyinime-rev/src/share/searchutility.cpp"
    "../src/ime_engine/googlepinyinime-rev/src/share/spellingtable.cpp"
    "../src/ime_engine/googlepinyinime-rev/src/share/spellingtrie.cpp"
    "../src/ime_engine/googlepinyinime-rev/src/share/splparser.cpp"
    "../src/ime_engine/googlepinyinime-rev/src/share/sync.cpp"
    "../src/ime_engine/googlepinyinime-rev/src/share/userdict.cpp"
    "../src/ime_engine/googlepinyinime-rev/src/share/utf16char.cpp"
    "../src/ime_engine/googlepinyinime-rev/src/share/utf16reader.cpp"
)

add_executable(${MY_EXECUTABLE_NAME} ${SOURCE_FILES} )

target_link_libraries(${MY_EXECUTABLE_NAME} fmt::fmt spdlog::spdlog unofficial::sqlite3::sqlite3 Boost::locale)
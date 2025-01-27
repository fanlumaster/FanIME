cmake_minimum_required(VERSION 3.25.0)

project(DeerWritingBrush LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_definitions(
    /D_UNICODE=1
    /DUNICODE=1
)

set(SQLITE_INCLUDE_DIR "./sqlite")
set(BOOST_INCLUDE_DIR "C:\\Users\\SonnyCalcr\\scoop\\apps\\boost\\1.87.0")

include_directories("./src")
include_directories("./src/Candidate")
include_directories("./src/Compartment")
include_directories("./src/Composition")
include_directories("./src/DictEngine")
include_directories("./src/DisplayAttribute")
include_directories("./src/Drawing")
include_directories("./src/Edit")
include_directories("./src/File")
include_directories("./src/Global")
include_directories("./src/Header")
include_directories("./src/IME")
include_directories("./src/Key")
include_directories("./src/LanguageBar")
include_directories("./src/Register")
include_directories("./src/Tf")
include_directories("./src/Thread")
include_directories("./src/Window")
include_directories(${SQLITE_INCLUDE_DIR})
include_directories(${BOOST_INCLUDE_DIR})

add_library(DeerWritingBrush SHARED
    # Candidate
    "./src/Candidate/CandidateListUIPresenter.cpp"
    "./src/Candidate/CandidateListUIPresenter.h"
    "./src/Candidate/EnumTfCandidates.cpp"
    "./src/Candidate/EnumTfCandidates.h"
    "./src/Candidate/SearchCandidateProvider.cpp"
    "./src/Candidate/SearchCandidateProvider.h"
    "./src/Candidate/TipCandidateList.cpp"
    "./src/Candidate/TipCandidateList.h"
    "./src/Candidate/TipCandidateString.cpp"
    "./src/Candidate/TipCandidateString.h"
    # Compartment
    "./src/Compartment/Compartment.cpp"
    "./src/Compartment/Compartment.h"
    # Composition
    "./src/Composition/Composition.cpp"
    "./src/Composition/CompositionProcessorEngine.cpp"
    "./src/Composition/CompositionProcessorEngine.cpp"
    "./src/Composition/CompositionProcessorEngine.h"
    "./src/Composition/EndComposition.cpp"
    "./src/Composition/StartComposition.cpp"
    # DictEngine
    "./src/DictEngine/BaseDictionaryEngine.cpp"
    "./src/DictEngine/BaseDictionaryEngine.h"
    "./src/DictEngine/DictionaryParser.cpp"
    "./src/DictEngine/DictionaryParser.h"
    "./src/DictEngine/DictionarySearch.cpp"
    "./src/DictEngine/DictionarySearch.h"
    "./src/DictEngine/TableDictionaryEngine.cpp"
    "./src/DictEngine/TableDictionaryEngine.h"
    # DisplayAttribute
    "./src/DisplayAttribute/DisplayAttribute.cpp"
    "./src/DisplayAttribute/DisplayAttributeInfo.cpp"
    "./src/DisplayAttribute/DisplayAttributeInfo.h"
    "./src/DisplayAttribute/DisplayAttributeProvider.cpp"
    "./src/DisplayAttribute/DisplayString.h"
    "./src/DisplayAttribute/EnumDisplayAttributeInfo.cpp"
    "./src/DisplayAttribute/EnumDisplayAttributeInfo.h"
    # Drawing
    "./src/Drawing/D2DSource.cpp"
    "./src/Drawing/D2DSource.h"
    # Edit
    "./src/Edit/EditSession.cpp"
    "./src/Edit/EditSession.h"
    "./src/Edit/GetTextExtentEditSession.cpp"
    "./src/Edit/GetTextExtentEditSession.h"
    "./src/Edit/TextEditSink.cpp"
    # File
    "./src/File/File.cpp"
    "./src/File/File.h"
    "./src/File/FileMapping.cpp"
    "./src/File/FileMapping.h"
    # Global
    "./src/Global/Globals.cpp"
    "./src/Global/Globals.h"
    # Header
    "./src/Header/Define.h"
    "./src/Header/Private.h"
    "./src/Header/resource.h"
    "./src/Header/stdafx.h"
    # IME
    "./src/IME/ActiveLanguageProfileNotifySink.cpp"
    "./src/IME/FunctionProviderSink.cpp"
    "./src/IME/SampleIME.cpp"
    "./src/IME/SampleIME.def"
    "./src/IME/SampleIME.h"
    "./src/IME/SampleIME.rc"
    "./src/IME/SampleIMEBaseStructure.cpp"
    "./src/IME/SampleIMEBaseStructure.h"
    "./src/IME/SampleIMEStructureArray.h"
    # Key
    "./src/Key/KeyEventSink.cpp"
    "./src/Key/KeyHandler.cpp"
    "./src/Key/KeyHandlerEditSession.cpp"
    "./src/Key/KeyHandlerEditSession.h"
    "./src/Key/KeyStateCategory.cpp"
    "./src/Key/KeyStateCategory.h"
    # LanguageBar
    "./src/LanguageBar/LanguageBar.cpp"
    "./src/LanguageBar/LanguageBar.h"
    # Register
    "./src/Register/Register.cpp"
    "./src/Register/RegKey.cpp"
    "./src/Register/RegKey.h"
    # Tf
    "./src/Tf/TfInputProcessorProfile.cpp"
    "./src/Tf/TfInputProcessorProfile.h"
    "./src/Tf/TfTextLayoutSink.cpp"
    "./src/Tf/TfTextLayoutSink.h"
    # Thread
    "./src/Thread/ThreadFocusSink.cpp"
    "./src/Thread/ThreadMgrEventSink.cpp"
    # Window
    "./src/Window/BaseWindow.cpp"
    "./src/Window/BaseWindow.h"
    "./src/Window/ButtonWindow.cpp"
    "./src/Window/ButtonWindow.h"
    "./src/Window/CandidateWindow.cpp"
    "./src/Window/CandidateWindow.h"
    "./src/Window/ScrollBarWindow.cpp"
    "./src/Window/ScrollBarWindow.h"
    "./src/Window/ShadowWindow.cpp"
    "./src/Window/ShadowWindow.h"
    # Main
    "./src/DllMain.cpp"
    "./src/Server.cpp"
    # Sqlite
    "./sqlite/sqlite3.c"
    "./sqlite/sqlite3.h"
    # Database
    "./src/DictEngine/FanDictionaryDbUtils.cpp"
    "./src/DictEngine/FanDictionaryDbUtils.h"
    # Pinyin Utils
    "./src/DictEngine/PinyinUtils.cpp"
    "./src/DictEngine/PinyinUtils.h"
)


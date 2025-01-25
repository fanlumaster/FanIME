// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved

#include "Private.h"
#include "TableDictionaryEngine.h"
#include "DictionarySearch.h"
#include "Globals.h"
#include <string>

//+---------------------------------------------------------------------------
//
// CollectWord
//
//----------------------------------------------------------------------------

VOID CTableDictionaryEngine::CollectWord(_In_ CStringRange *pKeyCode,
                                         _Inout_ CSampleImeArray<CStringRange> *pWordStrings)
{
    CDictionaryResult *pdret = nullptr;
    CDictionarySearch dshSearch(_locale, _pDictionaryFile, pKeyCode);

    while (dshSearch.FindPhrase(&pdret))
    {
        for (UINT index = 0; index < pdret->_FindPhraseList.Count(); index++)
        {
            CStringRange *pPhrase = nullptr;
            pPhrase = pWordStrings->Append();
            if (pPhrase)
            {
                *pPhrase = *pdret->_FindPhraseList.GetAt(index);
            }
        }

        delete pdret;
        pdret = nullptr;
    }
}

VOID CTableDictionaryEngine::CollectWord(_In_ CStringRange *pKeyCode,
                                         _Inout_ CSampleImeArray<CCandidateListItem> *pItemList)
{
    CDictionarySearch dshSearch(_locale, _pDictionaryFile, pKeyCode);

    Global::LogMessageW(L"fany pKeyCode starts...");
    Global::LogWideString(pKeyCode->Get(), pKeyCode->GetLength());
    Global::LogMessageW(L"fany pKeyCode ends...");

    const WCHAR *hardcodedData[] = {L"梁子", L"量子", L"两字", L"毛笔"};

    // 获取硬编码数据的数量
    UINT hardcodedDataCount = 4;

    // 将硬编码数据添加到 candidateList 中
    for (UINT i = 0; i < hardcodedDataCount; i++)
    {
        CCandidateListItem *pLI = nullptr;
        pLI = pItemList->Append();
        if (pLI)
        {
            // 设置硬编码数据到 _ItemString 和 _FindKeyCode
            pLI->_ItemString.Set(hardcodedData[i], wcslen(hardcodedData[i]));
            pLI->_FindKeyCode.Set(hardcodedData[i], wcslen(hardcodedData[i]));
        }
    }

    if (pItemList->Count())
    {
        std::string sql_str = "select * from tbl_1_j where key = 'jx';";
        sqlite3_stmt *stmt;
        int exit = sqlite3_prepare_v2(_pDictionaryDb, sql_str.c_str(), -1, &stmt, 0);
        if (exit != SQLITE_OK)
        {
            // logger->error("sqlite3_prepare_v2 error.");
        }
        std::string key;
        std::string value;
        int weight;
        std::wstring temp;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            key = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
            temp = Global::string_to_wstring(key);
            Global::LogMessageW(temp.c_str());
            value = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)));
            temp = Global::string_to_wstring(value);
            Global::LogMessageW(temp.c_str());
            weight = sqlite3_column_int(stmt, 3);
            temp = Global::string_to_wstring(std::to_string(weight));
            Global::LogMessageW(temp.c_str());
        }
        sqlite3_finalize(stmt);

        std::string str = "你好世界！";
        std::wstring wstr = Global::string_to_wstring(str);
        Global::LogMessageW(L"fany itemPair starts...");
        Global::LogMessageW(wstr.c_str());
        Global::LogWideString(pItemList->GetAt(0)->_ItemString.Get(), pItemList->GetAt(0)->_ItemString.GetLength());
        Global::LogWideString(pItemList->GetAt(0)->_FindKeyCode.Get(), pItemList->GetAt(0)->_FindKeyCode.GetLength());
        Global::LogMessageW(L"fany itemPair ends.");
        Global::LogMessageW(L"============================");
    }

    if (!pItemList->Count())
    {
        CCandidateListItem *pLI = nullptr;
        pLI = pItemList->Append();
        // 使用用户输入的关键字作为默认值
        const WCHAR *userInput = pKeyCode->Get();          // 获取用户输入的关键字
        DWORD_PTR userInputLength = pKeyCode->GetLength(); // 获取用户输入的长度
        pLI->_ItemString.Set(userInput, userInputLength);  // 设置用户输入为默认值
        pLI->_FindKeyCode.Set(userInput, userInputLength);
    }
}

//+---------------------------------------------------------------------------
//
// CollectWordForWildcard
//
//----------------------------------------------------------------------------

VOID CTableDictionaryEngine::CollectWordForWildcard(_In_ CStringRange *pKeyCode,
                                                    _Inout_ CSampleImeArray<CCandidateListItem> *pItemList)
{
    CDictionaryResult *pdret = nullptr;
    CDictionarySearch dshSearch(_locale, _pDictionaryFile, pKeyCode);

    while (dshSearch.FindPhraseForWildcard(&pdret))
    {
        for (UINT iIndex = 0; iIndex < pdret->_FindPhraseList.Count(); iIndex++)
        {
            CCandidateListItem *pLI = nullptr;
            pLI = pItemList->Append();
            if (pLI)
            {
                pLI->_ItemString.Set(*pdret->_FindPhraseList.GetAt(iIndex));
                pLI->_FindKeyCode.Set(pdret->_FindKeyCode.Get(), pdret->_FindKeyCode.GetLength());
            }
        }

        delete pdret;
        pdret = nullptr;
    }

    if (!pItemList->Count())
    {
        CCandidateListItem *pLI = nullptr;
        pLI = pItemList->Append();
        // 使用用户输入的关键字作为默认值
        const WCHAR *userInput = pKeyCode->Get();          // 获取用户输入的关键字
        DWORD_PTR userInputLength = pKeyCode->GetLength(); // 获取用户输入的长度
        pLI->_ItemString.Set(userInput, userInputLength);  // 设置用户输入为默认值
        pLI->_FindKeyCode.Set(userInput, userInputLength);
    }
}

//+---------------------------------------------------------------------------
//
// CollectWordFromConvertedStringForWildcard
//
//----------------------------------------------------------------------------

VOID CTableDictionaryEngine::CollectWordFromConvertedStringForWildcard(
    _In_ CStringRange *pString, _Inout_ CSampleImeArray<CCandidateListItem> *pItemList)
{
    CDictionaryResult *pdret = nullptr;
    CDictionarySearch dshSearch(_locale, _pDictionaryFile, pString);

    while (dshSearch.FindConvertedStringForWildcard(&pdret)) // TAIL ALL CHAR MATCH
    {
        for (UINT index = 0; index < pdret->_FindPhraseList.Count(); index++)
        {
            CCandidateListItem *pLI = nullptr;
            pLI = pItemList->Append();
            if (pLI)
            {
                pLI->_ItemString.Set(*pdret->_FindPhraseList.GetAt(index));
                pLI->_FindKeyCode.Set(pdret->_FindKeyCode.Get(), pdret->_FindKeyCode.GetLength());
            }
        }

        delete pdret;
        pdret = nullptr;
    }

    if (!pItemList->Count())
    {
        CCandidateListItem *pLI = nullptr;
        pLI = pItemList->Append();
        // 使用用户输入的关键字作为默认值
        const WCHAR *userInput = pString->Get();          // 获取用户输入的关键字
        DWORD_PTR userInputLength = pString->GetLength(); // 获取用户输入的长度
        pLI->_ItemString.Set(userInput, userInputLength); // 设置用户输入为默认值
        pLI->_FindKeyCode.Set(userInput, userInputLength);
    }
}

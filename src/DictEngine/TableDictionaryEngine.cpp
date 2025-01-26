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
#include <vector>
#include "FanDictionaryDbUtils.h"

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
    FanDictionaryDb fanDictionaryDb(_pDictionaryDb);

    std::wstring keyCodeWString = L"";
    keyCodeWString.append(pKeyCode->Get(), pKeyCode->GetLength()); // 添加 pwch 的内容
    std::string keyCodeString = Global::wstring_to_string(keyCodeWString);

#ifdef FAN_DEBUG
    Global::LogMessageW(L"fany pKeyCode starts...");
    Global::LogWideString(pKeyCode->Get(), pKeyCode->GetLength());
    Global::LogMessageW(keyCodeWString.c_str());
    Global::LogMessageW(L"fany pKeyCode ends...");
#endif

    // 获取硬编码数据的数量
    UINT hardcodedDataCount = 4;

    Global::CandidateList = fanDictionaryDb.Generate(keyCodeString);
    Global::WStringCandidateList.clear();
    Global::FindKeyCode = keyCodeWString;
    for (UINT i = 0; i < Global::CandidateList.size(); i++)
    {
        FanDictionaryDb::DbWordItem curItem = Global::CandidateList[i];
        std::string itemString = std::get<1>(curItem);
        std::wstring itemWString = Global::string_to_wstring(itemString);
        Global::WStringCandidateList.push_back(itemWString);
    }

    // 将硬编码数据添加到 candidateList 中
    for (UINT i = 0; i < Global::WStringCandidateList.size(); i++)
    {
        const std::wstring &wstr = Global::WStringCandidateList[i];

        CCandidateListItem *pLI = nullptr;
        pLI = pItemList->Append();
        if (pLI)
        {
            pLI->_ItemString.Set(wstr.c_str(), wstr.size());
            pLI->_FindKeyCode.Set(Global::FindKeyCode.c_str(), Global::FindKeyCode.size());
        }
    }

    if (!pItemList->Count())
    {
        CCandidateListItem *pLI = nullptr;
        pLI = pItemList->Append();
        pLI->_ItemString.Set(Global::FindKeyCode.c_str(), Global::FindKeyCode.size());
        pLI->_FindKeyCode.Set(Global::FindKeyCode.c_str(), Global::FindKeyCode.size());
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

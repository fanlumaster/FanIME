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
    CDictionaryResult *pdret = nullptr;
    CDictionarySearch dshSearch(_locale, _pDictionaryFile, pKeyCode);

    Global::LogMessageW(L"fany pKeyCode starts...");
    Global::LogWideString(pKeyCode->Get(), pKeyCode->GetLength());
    Global::LogMessageW(L"fany pKeyCode ends...");

    while (dshSearch.FindPhrase(&pdret))
    {
        for (UINT iIndex = 0; iIndex < pdret->_FindPhraseList.Count(); iIndex++)
        {
            CCandidateListItem *pLI = nullptr;
            pLI = pItemList->Append();
            if (pLI)
            {
                pLI->_ItemString.Set(*pdret->_FindPhraseList.GetAt(iIndex));
                pLI->_FindKeyCode.Set(pdret->_FindKeyCode.Get(), pdret->_FindKeyCode.GetLength());
                Global::LogMessageW(L"fany itemPair starts...");
                Global::LogWideString(pLI->_ItemString.Get(), pLI->_ItemString.GetLength());
                Global::LogWideString(pLI->_FindKeyCode.Get(), pLI->_FindKeyCode.GetLength());
                Global::LogMessageW(L"fany itemPair ends.");
                Global::LogMessageW(L"============================");
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

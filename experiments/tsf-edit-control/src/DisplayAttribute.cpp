#include "DisplayAttribute.h"
#include <Windows.h>
#include <msctf.h>

ITfDisplayAttributeMgr *g_pdam = NULL;

void PrintColor(const TF_DA_COLOR &color)
{
    if (color.type == TF_CT_COLORREF)
    {
        COLORREF cr = color.cr;
        UNREFERENCED_PARAMETER(cr);
    }
}

void PrintDisplayAttribute(const TF_DISPLAYATTRIBUTE &da)
{
    UNREFERENCED_PARAMETER(da);
}

CDispAttrProps *GetDispAttrProps()
{
    IEnumGUID *pEnumProp = NULL;
    CDispAttrProps *pProps = NULL;
    ITfCategoryMgr *pcat;
    HRESULT hr = E_FAIL;
    if (SUCCEEDED(hr = CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                                        (void **)&pcat)))
    {
        hr = pcat->EnumItemsInCategory(GUID_TFCAT_DISPLAYATTRIBUTEPROPERTY, &pEnumProp);
        pcat->Release();
    }

    if (SUCCEEDED(hr) && pEnumProp)
    {
        GUID guidProp;
        pProps = new CDispAttrProps;

        pProps->Add(GUID_PROP_ATTRIBUTE);
        while (pEnumProp->Next(1, &guidProp, NULL) == S_OK)
        {
            if (!IsEqualGUID(guidProp, GUID_PROP_ATTRIBUTE))
                pProps->Add(guidProp);
        }
    }

    if (pEnumProp)
        pEnumProp->Release();

    return pProps;
}

HRESULT InitDisplayAttrbute()
{
    if (g_pdam)
        g_pdam->Release();

    g_pdam = NULL;

    if (FAILED(CoCreateInstance(CLSID_TF_DisplayAttributeMgr, NULL, CLSCTX_INPROC_SERVER, IID_ITfDisplayAttributeMgr,
                                (void **)&g_pdam)))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT UninitDisplayAttrbute()
{
    if (g_pdam)
    {
        g_pdam->Release();
        g_pdam = NULL;
    }

    return S_OK;
}

HRESULT GetDisplayAttributeTrackPropertyRange(TfEditCookie ec, ITfContext *pic, ITfRange *pRange,
                                              ITfReadOnlyProperty **ppProp, CDispAttrProps *pDispAttrProps)
{
    ITfReadOnlyProperty *pProp = NULL;
    HRESULT hr = E_FAIL;
    GUID *pguidProp = NULL;
    const GUID **ppguidProp;
    ULONG ulNumProp = 0;
    ULONG i;

    if (!pDispAttrProps)
        goto Exit;

    pguidProp = pDispAttrProps->GetPropTable();
    if (!pguidProp)
        goto Exit;

    ulNumProp = pDispAttrProps->Count();
    if (!ulNumProp)
        goto Exit;

    if ((ppguidProp = (const GUID **)LocalAlloc(LMEM_ZEROINIT, sizeof(GUID *) * ulNumProp)) == NULL)
        return E_OUTOFMEMORY;

    for (i = 0; i < ulNumProp; i++)
    {
        ppguidProp[i] = pguidProp++;
    }

    if (SUCCEEDED(hr = pic->TrackProperties(ppguidProp, ulNumProp, 0, NULL, &pProp)))
    {
        *ppProp = pProp;
    }

    LocalFree(ppguidProp);

Exit:
    return hr;
}

HRESULT GetDisplayAttributeData(TfEditCookie ec, ITfReadOnlyProperty *pProp, ITfRange *pRange, TF_DISPLAYATTRIBUTE *pda,
                                TfGuidAtom *pguid)
{
    VARIANT var;
    IEnumTfPropertyValue *pEnumPropertyVal = NULL;
    TF_PROPERTYVAL tfPropVal = {};
    GUID guid;
    TfGuidAtom gaVal = TF_INVALID_GUIDATOM;
    ITfDisplayAttributeInfo *pDAI = NULL;

    HRESULT hr = E_FAIL;
    VariantInit(&var);

    ITfCategoryMgr *pcat = NULL;
    if (FAILED(hr = CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                                     (void **)&pcat)))
    {
        return hr;
    }

    hr = S_FALSE;
    if (SUCCEEDED(pProp->GetValue(ec, pRange, &var)))
    {
        if ((var.vt == VT_UNKNOWN) && var.punkVal &&
            SUCCEEDED(var.punkVal->QueryInterface(IID_IEnumTfPropertyValue, (void **)&pEnumPropertyVal)))
        {
            while (pEnumPropertyVal->Next(1, &tfPropVal, NULL) == S_OK)
            {
                if (tfPropVal.varValue.vt == VT_EMPTY)
                    continue;

                if ((tfPropVal.varValue.vt != VT_I4) && (tfPropVal.varValue.vt != VT_UI4))
                {
                    VariantClear(&tfPropVal.varValue);
                    continue;
                }

                gaVal = (TfGuidAtom)tfPropVal.varValue.lVal;

                pcat->GetGUID(gaVal, &guid);

                if ((g_pdam != NULL) && SUCCEEDED(g_pdam->GetDisplayAttributeInfo(guid, &pDAI, NULL)))
                {
                    if (pda)
                    {
                        pDAI->GetAttributeInfo(pda);
                        PrintDisplayAttribute(*pda);
                    }

                    if (pguid)
                    {
                        *pguid = gaVal;
                    }

                    pDAI->Release();
                    hr = S_OK;
                    VariantClear(&tfPropVal.varValue);
                    break;
                }

                VariantClear(&tfPropVal.varValue);
            }
            pEnumPropertyVal->Release();
        }
        else if ((var.vt == VT_I4) || (var.vt == VT_UI4))
        {
            gaVal = (TfGuidAtom)var.lVal;
            if (SUCCEEDED(pcat->GetGUID(gaVal, &guid)) && (g_pdam != NULL) &&
                SUCCEEDED(g_pdam->GetDisplayAttributeInfo(guid, &pDAI, NULL)))
            {
                if (pda)
                {
                    pDAI->GetAttributeInfo(pda);
                    PrintDisplayAttribute(*pda);
                }

                if (pguid)
                {
                    *pguid = gaVal;
                }

                pDAI->Release();
                hr = S_OK;
            }
        }
        VariantClear(&var);
    }

    pcat->Release();

    return hr;
}

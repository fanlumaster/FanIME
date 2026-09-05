#include "TsfTextServices.h"

#include "DisplayAttribute.h"

HINSTANCE g_hInst = NULL;
ITfThreadMgr *g_pThreadMgr = NULL;
TfClientId g_TfClientId = TF_CLIENTID_NULL;

namespace
{
LONG g_serviceRefCount = 0;
}

BOOL InitializeTsfTextServices(HINSTANCE hInstance)
{
    if (InterlockedIncrement(&g_serviceRefCount) > 1)
    {
        g_hInst = hInstance;
        return TRUE;
    }

    g_hInst = hInstance;

    if (FAILED(CoCreateInstance(CLSID_TF_ThreadMgr, NULL, CLSCTX_INPROC_SERVER, IID_ITfThreadMgr,
                                reinterpret_cast<void **>(&g_pThreadMgr))))
    {
        InterlockedDecrement(&g_serviceRefCount);
        return FALSE;
    }

    if (FAILED(g_pThreadMgr->Activate(&g_TfClientId)))
    {
        g_pThreadMgr->Release();
        g_pThreadMgr = NULL;
        g_TfClientId = TF_CLIENTID_NULL;
        InterlockedDecrement(&g_serviceRefCount);
        return FALSE;
    }

    if (FAILED(InitDisplayAttrbute()))
    {
        g_pThreadMgr->Deactivate();
        g_pThreadMgr->Release();
        g_pThreadMgr = NULL;
        g_TfClientId = TF_CLIENTID_NULL;
        InterlockedDecrement(&g_serviceRefCount);
        return FALSE;
    }

    return TRUE;
}

void UninitializeTsfTextServices()
{
    LONG refCount = InterlockedDecrement(&g_serviceRefCount);
    if (refCount > 0)
    {
        return;
    }

    if (refCount < 0)
    {
        g_serviceRefCount = 0;
        return;
    }

    UninitDisplayAttrbute();

    if (g_pThreadMgr)
    {
        g_pThreadMgr->Deactivate();
        g_pThreadMgr->Release();
        g_pThreadMgr = NULL;
    }

    g_TfClientId = TF_CLIENTID_NULL;
    g_hInst = NULL;
}

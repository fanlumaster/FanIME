#pragma once
#include <string>
#include <windows.h>
#include <sciter-x-api.h>

inline std::wstring HTMLString = LR"()";
inline std::wstring BodyString = LR"()";
inline std::wstring CandStr = L"";

int PrepareCandidateWindowSciterHtml();
void UpdateBodyContent(HWND hwnd, const wchar_t *newContent);
void InflateCandidateWindowSciter(std::wstring &str);

class SciterBridgeJs : public sciter::om::asset<SciterBridgeJs>
{
  public:
    sciter::string my_msg = WSTR("msg from cpp");

    SciterBridgeJs()
    {
    }

    void adjustInitialWindowSize(sciter::value width, sciter::value height);
    void preserveWindowSize(sciter::value index, sciter::value width, sciter::value height);

    SOM_PASSPORT_BEGIN(SciterBridgeJs)
    SOM_FUNCS(                             //
        SOM_FUNC(adjustInitialWindowSize), //
        SOM_FUNC(preserveWindowSize)       //
    )
    SOM_PROPS(SOM_RO_PROP(my_msg))
    SOM_PASSPORT_END
};
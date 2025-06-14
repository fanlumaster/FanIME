#include <string>
#include <filesystem>
#include <boost/locale.hpp>
#include <fstream>
#include <fmt/xchar.h>
#include <sciter-x-api.h>
#include "defines/globals.h"
#include "utils/common_utils.h"
#include "candidate_window_sciter.h"
#include "global/globals.h"

std::wstring ReadHtmlFile(const std::wstring &filePath)
{
    std::wifstream file(filePath);
    if (!file)
    {
        // TODO: Log
        return L"";
    }
    // Use Boost Locale to handle UTF-8
    file.imbue(boost::locale::generator().generate("en_US.UTF-8"));
    std::wstringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int PrepareCandidateWindowSciterHtml()
{
    std::wstring entireHtml = fmt::format(                                            //
        L"{}\\{}\\html\\sciter\\default-themes\\vertical_candidate_window_dark.html", //
        string_to_wstring(CommonUtils::get_local_appdata_path()),                     //
        GlobalIme::AppName                                                            //
    );
    std::wstring bodyHtml = fmt::format(                                                    //
        L"{}\\{}\\html\\sciter\\default-themes\\body\\vertical_candidate_window_dark.html", //
        string_to_wstring(CommonUtils::get_local_appdata_path()),                           //
        GlobalIme::AppName                                                                  //
    );

    bool isHorizontal = false;
    bool isNormal = true;

    if (isHorizontal)
    {
        entireHtml = L"/html/sciter/default-themes/horizontal_candidate_window_dark.html";
        bodyHtml = L"/html/sciter/default-themes/body/horizontal_candidate_window_dark.html";
        if (isNormal)
        {
            entireHtml = L"/html/sciter/default-themes/horizontal_candidate_window_dark_normal.html";
            bodyHtml = L"/html/sciter/default-themes/body/horizontal_candidate_window_dark_normal.html";
        }
    }

    std::wstring htmlPath = entireHtml;
    std::wstring bodyPath = bodyHtml;
    ::BodyString = ReadHtmlFile(bodyPath);
    OutputDebugString(bodyPath.c_str());

    return 0;
}

void InflateCandidateWindowSciter(std::wstring &str)
{
    std::wstringstream wss(str);
    std::wstring token;
    std::vector<std::wstring> words;

    while (std::getline(wss, token, L','))
    {
        words.push_back(token);
    }

    int size = words.size();

    while (words.size() < 9)
    {
        words.push_back(L"");
    }

    std::wstring result = fmt::format( //
        ::BodyString,                  //
        words[0],                      //
        words[1],                      //
        words[2],                      //
        words[3],                      //
        words[4],                      //
        words[5],                      //
        words[6],                      //
        words[7],                      //
        words[8]                       //
    );                                 //

    if (size < 9)
    {
        size_t pos = result.find(fmt::format(L"<!--{}Anchor-->", size));
        result = result.substr(0, pos) + L"</div>";
    }

    UpdateBodyContent(::global_hwnd, result.c_str());
}

SBOOL SC_CALLBACK UpdateBodyCallback(HELEMENT he, LPVOID param)
{
    const wchar_t *newContent = (const wchar_t *)param;
    std::wstring ws(newContent);
    std::string utf8Content = wstring_to_string(ws);
    SciterSetElementHtml(                  //
        he,                                //
        (const BYTE *)utf8Content.c_str(), //
        (UINT)utf8Content.size(),          //
        SIH_REPLACE_CONTENT                // Will not damage dom css
    );
    return FALSE;
}

void UpdateBodyContent(HWND hwnd, const wchar_t *newContent)
{
    HELEMENT root = 0;
    if (SciterGetRootElement(hwnd, &root) != SCDOM_OK || !root)
        return;
    // Do not trigger hover effect when window first show
    SciterCallScriptingFunction(root, "ClearState", nullptr, 0, nullptr);
    SciterSelectElementsW(  //
        root,               //
        WSTR("body"),       //
        UpdateBodyCallback, //
        (LPVOID)newContent  //
    );
}
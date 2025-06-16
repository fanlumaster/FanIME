#include <string>
#include <boost/locale.hpp>
#include <fstream>
#include <fmt/xchar.h>
#include <sciter-x-api.h>
#include "defines/globals.h"
#include "utils/common_utils.h"
#include "candidate_window_sciter.h"
#include "global/globals.h"
#include <utf8.h>
#include "utils/window_utils.h"
#include "defines/globals.h"

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

    int i = 0;
    int maxCnt = 2;
    while (std::getline(wss, token, L','))
    {
        words.push_back(token);
        i++;
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

int AdjustWndPosition( //
    HWND hwnd,         //
    int crateX,        //
    int crateY,        //
    int width,         //
    int height,        //
    int properPos[2]   //
)
{
    properPos[0] = crateX;
    properPos[1] = crateY + 3;
    MonitorCoordinates coordinates = GetMonitorCoordinates();
    if (properPos[0] < coordinates.left)
    {
        properPos[0] = coordinates.left + 2;
    }
    if (properPos[1] < coordinates.top)
    {
        properPos[1] = coordinates.top + 2;
    }
    if (properPos[0] + width > coordinates.right)
    {
        properPos[0] = coordinates.right - width - 2;
    }
    if (properPos[1] + ::cand_window_height_array[7] > coordinates.bottom)
    {
        properPos[1] = properPos[1] - height - 30 - 2;
    }
    return 0;
}

void SciterBridgeJs::adjustInitialWindowSize(sciter::value width, sciter::value height)
{
    int realWidth = 0;
    if (width.is_int())
    {
        realWidth = width.get<int>();
    }
    int realHeight = 0;
    if (height.is_int())
    {
        realHeight = height.get<int>();
    }
#ifdef FANY_DEBUG
    OutputDebugString(fmt::format(L"Candidate Window size: {} {}", realWidth, realHeight).c_str());
#endif
    // TODO: Adjust window size
    ::CANDIDATE_WINDOW_WIDTH = realWidth;
    ::DEFAULT_WINDOW_WIDTH = realWidth;
    ::CANDIDATE_WINDOW_HEIGHT = realHeight;
    ::cand_window_width_array[0] = realWidth;
    ::cand_window_width_array[1] = realWidth;
    UINT uflags = SWP_NOZORDER | SWP_NOMOVE;
    SetWindowPos(                    //
        ::global_hwnd,               //
        nullptr,                     //
        0,                           //
        0,                           //
        realWidth + ::SHADOW_WIDTH,  //
        realHeight + ::SHADOW_WIDTH, //
        uflags                       //
    );
}

void SciterBridgeJs::preserveWindowSize(sciter::value index, sciter::value width, sciter::value height)
{
    int curIndex = 0;
    if (index.is_int())
    {
        curIndex = index.get<int>();
    }
    int realWidth = 0;
    if (width.is_int())
    {
        realWidth = width.get<int>();
    }
    if (curIndex < ::MAX_HAN_CHARS)
    {
        ::cand_window_width_array[curIndex] = realWidth;
    }
    int realHeight = 0;
    if (height.is_int())
    {
        realHeight = height.get<int>();
    }
    if (curIndex < ::MAX_HAN_CHARS)
    {
        ::cand_window_height_array[curIndex - 2] = realHeight;
    }
}
#pragma once

#include <string>
#include <vector>

inline const std::vector<std::wstring> FANY_IME_EVENT_ARRAY = {
    L"FanyImeKeyEvent" // Event sent to UI process to notify time to update UI by new pinyin_string
};

void EventListenerLoopThread();
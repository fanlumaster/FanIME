#include "MetasequoiaImeEngine/user_dictionary/user_dictionary_journal.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <windows.h>

int wmain(int argc, wchar_t **argv)
{
    std::filesystem::path data_dir =
        std::filesystem::path(user_dictionary::default_user_db_path()).parent_path();
    for (int i = 1; i < argc; ++i)
    {
        if (std::wstring(argv[i]) == L"--data-dir" && i + 1 < argc)
        {
            data_dir = argv[++i];
        }
        else
        {
            std::wcerr << L"Usage: MetasequoiaImeDictionaryReplay.exe [--data-dir <directory>]\n";
            return 2;
        }
    }

    const auto result = user_dictionary::replay(
        (data_dir / "msime_user.db").string(),
        (data_dir / "msime.db").string(),
        (data_dir / "english.db").string());
    if (!result.error.empty())
    {
        std::cerr << result.error << '\n';
        return 1;
    }
    std::cout << "Applied " << result.applied << " user dictionary operations.\n";
    return 0;
}

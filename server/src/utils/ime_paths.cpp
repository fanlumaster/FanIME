#include "common_utils.h"

#include <aclapi.h>
#include <filesystem>
#include <fstream>
#include <sddl.h>
#include <shlobj.h>
#include <vector>

namespace
{
constexpr wchar_t kAppName[] = L"metasequoiaime";

bool PathHasEmptyComponent(const std::wstring &path)
{
    if (path.empty())
    {
        return true;
    }
    size_t index = 0;
    if (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\')
    {
        index = 2;
    }
    else if (path[0] == L'\\')
    {
        return true;
    }
    for (; index + 1 < path.size(); ++index)
    {
        if (path[index] == L'\\' && path[index + 1] == L'\\')
        {
            return true;
        }
    }
    return false;
}

bool IsUsableAbsolutePath(const std::wstring &path)
{
    if (PathHasEmptyComponent(path))
    {
        return false;
    }
    if (path.size() >= 2 && path[1] == L':')
    {
        return true;
    }
    return path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\';
}

std::wstring QueryEnvironmentW(const wchar_t *name)
{
    const DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
    if (needed == 0)
    {
        return {};
    }
    std::wstring value(needed, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, value.data(), needed);
    if (written == 0 || written >= needed)
    {
        return {};
    }
    value.resize(written);
    return value;
}

std::wstring QueryKnownFolder(REFKNOWNFOLDERID folder_id)
{
    PWSTR known_path = nullptr;
    if (FAILED(SHGetKnownFolderPath(folder_id, KF_FLAG_DEFAULT, nullptr, &known_path)) || !known_path)
    {
        return {};
    }
    std::wstring result(known_path);
    CoTaskMemFree(known_path);
    return result;
}

bool DirectoryIsWritable(const std::wstring &path)
{
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec)
    {
        return false;
    }
    const std::filesystem::path probe = std::filesystem::path(path) / L".ime-write-probe";
    {
        std::ofstream out(probe, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            return false;
        }
    }
    std::filesystem::remove(probe, ec);
    return true;
}

void EnsureMediumIntegrityWritableDirectory(const std::wstring &path)
{
    std::error_code ec;
    std::filesystem::create_directories(path, ec);

    PSID users_sid = nullptr;
    if (ConvertStringSidToSidW(L"S-1-5-32-545", &users_sid))
    {
        EXPLICIT_ACCESSW access{};
        access.grfAccessPermissions = GENERIC_ALL;
        access.grfAccessMode = GRANT_ACCESS;
        access.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        access.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        access.Trustee.ptstrName = static_cast<LPWSTR>(users_sid);

        PACL old_dacl = nullptr;
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        PACL new_dacl = nullptr;
        if (GetNamedSecurityInfoW(path.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &old_dacl,
                                  nullptr, &descriptor) == ERROR_SUCCESS)
        {
            if (SetEntriesInAclW(1, &access, old_dacl, &new_dacl) == ERROR_SUCCESS)
            {
                SetNamedSecurityInfoW(const_cast<wchar_t *>(path.c_str()), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                      nullptr, nullptr, new_dacl, nullptr);
                LocalFree(new_dacl);
            }
            LocalFree(descriptor);
        }
        LocalFree(users_sid);
    }

    PSID medium_sid = nullptr;
    if (!ConvertStringSidToSidW(L"S-1-16-8192", &medium_sid))
    {
        return;
    }
    const DWORD sacl_size = sizeof(ACL) + GetLengthSid(medium_sid) + sizeof(SYSTEM_MANDATORY_LABEL_ACE) + 32;
    std::vector<BYTE> sacl_buffer(sacl_size);
    auto *sacl = reinterpret_cast<PACL>(sacl_buffer.data());
    if (InitializeAcl(sacl, sacl_size, ACL_REVISION) &&
        AddMandatoryAce(sacl, ACL_REVISION, 0, SYSTEM_MANDATORY_LABEL_NO_WRITE_UP, medium_sid))
    {
        SetNamedSecurityInfoW(const_cast<wchar_t *>(path.c_str()), SE_FILE_OBJECT, LABEL_SECURITY_INFORMATION, nullptr,
                              nullptr, nullptr, sacl);
    }
    LocalFree(medium_sid);
}
} // namespace

namespace CommonUtils
{
std::wstring get_local_appdata_path_w()
{
    const std::wstring from_env = QueryEnvironmentW(L"LOCALAPPDATA");
    if (IsUsableAbsolutePath(from_env))
    {
        return from_env;
    }

    const std::wstring known = QueryKnownFolder(FOLDERID_LocalAppData);
    if (IsUsableAbsolutePath(known))
    {
        return known;
    }
    return {};
}

std::wstring get_ime_data_path_w()
{
    return get_local_appdata_path_w() + L"\\" + kAppName;
}

std::wstring get_webview2_user_data_path(const std::wstring &folder_name)
{
    std::wstring program_data = QueryKnownFolder(FOLDERID_ProgramData);
    if (!IsUsableAbsolutePath(program_data))
    {
        program_data = QueryEnvironmentW(L"ProgramData");
    }
    if (!IsUsableAbsolutePath(program_data))
    {
        program_data = L"C:\\ProgramData";
    }
    const std::wstring root = program_data + L"\\" + kAppName;
    const std::wstring path = root + L"\\" + folder_name;
    EnsureMediumIntegrityWritableDirectory(root);
    EnsureMediumIntegrityWritableDirectory(path);
    if (DirectoryIsWritable(path))
    {
        return path;
    }

    const std::wstring local = get_local_appdata_path_w();
    if (!IsUsableAbsolutePath(local))
    {
        return path;
    }
    return local + L"\\" + kAppName + L"\\" + folder_name;
}
} // namespace CommonUtils

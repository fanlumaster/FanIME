#include "skin/candidate_skin_catalog.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <system_error>

namespace CandidateSkinCatalog
{
namespace
{
using Json = nlohmann::json;

void SetError(std::string *error, const std::string &message)
{
    if (error) *error = message;
}

bool IsSafeFileName(const std::string &name, const std::string &extension)
{
    if (name.empty() || name.size() > 128 ||
        name.size() <= extension.size() || name.substr(name.size() - extension.size()) != extension)
        return false;
    return std::all_of(name.begin(), name.end(), [](unsigned char ch) {
        return std::isalnum(ch) || ch == '.' || ch == '_' || ch == '-';
    });
}

bool IsSafeRelativeResource(const std::string &name)
{
    if (name.empty() || name.size() > 256 || name.front() == '/' || name.front() == '\\' ||
        name.find('\\') != std::string::npos)
        return false;
    if (!std::all_of(name.begin(), name.end(), [](unsigned char ch) {
            return std::isalnum(ch) || ch == '/' || ch == '.' || ch == '_' || ch == '-';
        })) return false;
    std::filesystem::path path(name);
    if (path.is_absolute()) return false;
    for (const auto &part : path)
        if (part == ".." || part == "." || part.empty()) return false;
    return true;
}

bool ReadString(const Json &root, const char *key, std::string &out, size_t maximum, bool required)
{
    const auto it = root.find(key);
    if (it == root.end()) return !required;
    if (!it->is_string()) return false;
    out = it->get<std::string>();
    return (!required || !out.empty()) && out.size() <= maximum;
}

bool ReadEnumArray(const Json &root, const char *key, const std::vector<std::string> &allowed,
                   std::vector<std::string> &out)
{
    const auto it = root.find(key);
    if (it == root.end() || !it->is_array() || it->empty()) return false;
    for (const auto &item : *it)
    {
        if (!item.is_string()) return false;
        const std::string value = item.get<std::string>();
        if (std::find(allowed.begin(), allowed.end(), value) == allowed.end() ||
            std::find(out.begin(), out.end(), value) != out.end())
            return false;
        out.push_back(value);
    }
    return true;
}

double BoundedNumber(const Json &root, const char *key, double maximum)
{
    const auto it = root.find(key);
    if (it == root.end()) return 0.0;
    if (!it->is_number()) return -1.0;
    const double value = it->get<double>();
    return std::isfinite(value) && value >= 0.0 && value <= maximum ? value : -1.0;
}
} // namespace

bool IsBuiltIn(const std::string &id)
{
    return id == "fluent" || id == "wechat" || id == "graphite" || id == "willow_green";
}

bool IsSafeId(const std::string &id)
{
    if (id.empty() || id.size() > 64 || !std::isalnum(static_cast<unsigned char>(id.front()))) return false;
    return std::all_of(id.begin(), id.end(), [](unsigned char ch) {
        return std::islower(ch) || std::isdigit(ch) || ch == '.' || ch == '_' || ch == '-';
    });
}

bool Supports(const Package &package, const std::string &layout, const std::string &theme)
{
    return std::find(package.layouts.begin(), package.layouts.end(), layout) != package.layouts.end() &&
           std::find(package.themes.begin(), package.themes.end(), theme) != package.themes.end();
}

std::optional<Package> Load(const std::filesystem::path &skinsRoot, const std::string &id, std::string *error)
{
    if (!IsSafeId(id) || IsBuiltIn(id))
    {
        SetError(error, "目录名不是有效的外部皮肤 ID");
        return std::nullopt;
    }
    const std::filesystem::path directory = skinsRoot / std::filesystem::u8path(id);
    std::ifstream stream(directory / L"skin.json", std::ios::binary);
    if (!stream)
    {
        SetError(error, "缺少 skin.json");
        return std::nullopt;
    }
    try
    {
        const Json root = Json::parse(stream);
        if (!root.is_object() || root.value("schemaVersion", 0) != 1)
        {
            SetError(error, "仅支持 schemaVersion 1");
            return std::nullopt;
        }
        Package package;
        if (!ReadString(root, "id", package.id, 64, true) || package.id != id ||
            !ReadString(root, "name", package.name, 80, true) ||
            !ReadString(root, "version", package.version, 32, true) ||
            !ReadString(root, "author", package.author, 120, false) ||
            !ReadString(root, "description", package.description, 500, false) ||
            !ReadString(root, "base", package.base, 32, true) || !IsBuiltIn(package.base) ||
            !ReadString(root, "stylesheet", package.stylesheet, 128, true) ||
            !IsSafeFileName(package.stylesheet, ".css"))
        {
            SetError(error, "manifest 的基本信息或文件名无效");
            return std::nullopt;
        }
        if (root.contains("preview") &&
            (!ReadString(root, "preview", package.preview, 256, false) ||
             !IsSafeRelativeResource(package.preview)))
        {
            SetError(error, "preview 必须是皮肤目录内的相对路径");
            return std::nullopt;
        }
        if (!root.contains("supports") || !root["supports"].is_object() ||
            !ReadEnumArray(root["supports"], "layouts", {"horizontal", "vertical"}, package.layouts) ||
            !ReadEnumArray(root["supports"], "themes", {"dark", "light"}, package.themes))
        {
            SetError(error, "supports.layouts 或 supports.themes 无效");
            return std::nullopt;
        }
        if (!root.contains("candidateWindow") || !root["candidateWindow"].is_object())
        {
            SetError(error, "缺少 candidateWindow");
            return std::nullopt;
        }
        const Json &window = root["candidateWindow"];
        package.minWidthDip = BoundedNumber(window, "minWidthDip", 1000.0);
        if (package.minWidthDip < 0.0)
        {
            SetError(error, "candidateWindow.minWidthDip 超出范围");
            return std::nullopt;
        }
        if (!window.contains("decoration") || !window["decoration"].is_object())
        {
            SetError(error, "缺少 candidateWindow.decoration");
            return std::nullopt;
        }
        package.decorationTopDip = BoundedNumber(window["decoration"], "topInsetDip", 500.0);
        package.decorationWidthDip = BoundedNumber(window["decoration"], "widthDip", 1000.0);
        if (package.decorationTopDip < 0.0 || package.decorationWidthDip < 0.0 ||
            ((package.decorationTopDip == 0.0) != (package.decorationWidthDip == 0.0)))
        {
            SetError(error, "decoration 尺寸无效");
            return std::nullopt;
        }
        std::error_code ec;
        if (!std::filesystem::is_regular_file(directory / std::filesystem::u8path(package.stylesheet), ec))
        {
            SetError(error, "找不到 stylesheet 文件");
            return std::nullopt;
        }
        // Preview is settings-only. A missing thumbnail must not disable the live
        // candidate skin; CSS can still apply, and url() failures stay local.
        return package;
    }
    catch (const std::exception &)
    {
        SetError(error, "skin.json 不是有效的 JSON manifest");
        return std::nullopt;
    }
}

ScanResult Scan(const std::filesystem::path &skinsRoot)
{
    ScanResult result;
    std::error_code ec;
    if (!std::filesystem::exists(skinsRoot, ec)) return result;
    for (std::filesystem::directory_iterator it(skinsRoot, ec), end; !ec && it != end; it.increment(ec))
    {
        if (!it->is_directory(ec)) continue;
        const std::string folder = it->path().filename().u8string();
        std::string error;
        auto package = Load(skinsRoot, folder, &error);
        if (package) result.packages.push_back(std::move(*package));
        else result.issues.push_back({folder, error});
    }
    if (ec) result.issues.push_back({"skins", "无法完整读取皮肤目录"});
    std::sort(result.packages.begin(), result.packages.end(), [](const Package &a, const Package &b) {
        return a.name < b.name;
    });
    std::sort(result.issues.begin(), result.issues.end(), [](const Issue &a, const Issue &b) {
        return a.folder < b.folder;
    });
    return result;
}
} // namespace CandidateSkinCatalog

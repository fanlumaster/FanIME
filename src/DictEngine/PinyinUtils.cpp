#include "PinyinUtils.h"
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cctype>
#include <algorithm>
#include <memory>
#include <boost/algorithm/string.hpp>

std::string PinyinUtils::GetLocalAppdataPath()
{
    char *localAppDataDir = nullptr;
    std::string localAppDataDirStr;

    errno_t err = _dupenv_s(&localAppDataDir, nullptr, "LOCALAPPDATA");
    if (err == 0 && localAppDataDir != nullptr)
    {
        localAppDataDirStr = std::string(localAppDataDir);
    }

    std::unique_ptr<char, decltype(&free)> dirPtr(localAppDataDir, free);

    return localAppDataDirStr.empty() ? "" : localAppDataDirStr;
}

std::string PinyinUtils::LocalAppdataPath = PinyinUtils::GetLocalAppdataPath();

std::unordered_map<std::string, std::string> PinyinUtils::SmKeymaps{{"sh", "u"}, {"ch", "i"}, {"zh", "v"}};
std::unordered_map<std::string, std::string> PinyinUtils::SmKeymapsReversed{{"u", "sh"}, {"i", "ch"}, {"v", "zh"}};
std::unordered_map<std::string, std::string> PinyinUtils::ZeroSmKeymaps{
    {"a", "aa"},  {"ai", "ai"},  {"ao", "ao"}, {"ang", "ah"}, {"e", "ee"}, {"ei", "ei"},
    {"en", "en"}, {"eng", "eg"}, {"er", "er"}, {"o", "oo"},   {"ou", "ou"}};
std::unordered_map<std::string, std::string> PinyinUtils::ZeroSmKeymapsReversed{
    {"aa", "a"},  {"ai", "ai"}, {"an", "an"},  {"ao", "ao"}, {"ah", "ang"}, {"ee", "e"},
    {"ei", "ei"}, {"en", "en"}, {"eg", "eng"}, {"er", "er"}, {"oo", "o"},   {"ou", "ou"}};
std::unordered_map<std::string, std::string> PinyinUtils::YmKeymaps{
    {"iu", "q"},   {"ei", "w"},   {"e", "e"},    {"uan", "r"}, {"ue", "t"},  {"ve", "t"}, {"un", "y"},
    {"u", "u"},    {"i", "i"},    {"uo", "o"},   {"o", "o"},   {"ie", "p"},  {"a", "a"},  {"ong", "s"},
    {"iong", "s"}, {"ai", "d"},   {"en", "f"},   {"eng", "g"}, {"ang", "h"}, {"an", "j"}, {"uai", "k"},
    {"ing", "k"},  {"uang", "l"}, {"iang", "l"}, {"ou", "z"},  {"ua", "x"},  {"ia", "x"}, {"ao", "c"},
    {"ui", "v"},   {"v", "v"},    {"in", "b"},   {"iao", "n"}, {"ian", "m"}};
std::unordered_map<std::string, std::string> PinyinUtils::YmKeymapsReversed{
    {"q", "iu"},  {"w", "ei"},  {"e", "e"},  {"r", "uan"}, {"t", "ve"},   {"y", "un"}, {"u", "u"},
    {"i", "i"},   {"o", "o"},   {"p", "ie"}, {"a", "a"},   {"s", "iong"}, {"d", "ai"}, {"f", "en"},
    {"g", "eng"}, {"h", "ang"}, {"j", "an"}, {"k", "ing"}, {"l", "iang"}, {"z", "ou"}, {"x", "ia"},
    {"c", "ao"},  {"v", "v"},   {"b", "in"}, {"n", "iao"}, {"m", "ian"}};

std::unordered_set<std::string> &InitializeQuanpinSet()
{
    static std::unordered_set<std::string> tmpSet;
    std::ifstream pinyinPath(PinyinUtils::GetLocalAppdataPath() + "\\DeerWritingBrush\\pinyin.txt");
    std::string line;
    while (std::getline(pinyinPath, line))
    {
        line.erase(std::remove_if(line.begin(), line.end(), [](unsigned char x) { return std::isspace(x); }),
                   line.end());
        tmpSet.insert(line);
    }
    return tmpSet;
}
std::unordered_set<std::string> &PinyinUtils::QuanpinSet = InitializeQuanpinSet();

std::unordered_map<std::string, std::string> &InitializeHelpcodeKeymap()
{
    static std::unordered_map<std::string, std::string> tmpMap;
    std::ifstream helpcodePath(PinyinUtils::GetLocalAppdataPath() + "\\DeerWritingBrush\\helpcode.txt");
    std::string line;
    while (std::getline(helpcodePath, line))
    {
        size_t pos = line.find('=');
        tmpMap[line.substr(0, pos)] = line.substr(pos + 1, 2);
    }
    return tmpMap;
}
std::unordered_map<std::string, std::string> &PinyinUtils::HelpcodeKeymap = InitializeHelpcodeKeymap();

std::string PinyinUtils::CvtSingleShuangpinToQuanpin(std::string spStr)
{
    if (ZeroSmKeymapsReversed.count(spStr) > 0)
    {
        return ZeroSmKeymapsReversed[spStr];
    }
    if (spStr.size() != 2)
        return "";
    std::string res = "";
    std::string sm;
    std::vector<std::string> ymList;

    if (SmKeymapsReversed.count(spStr.substr(0, 1)) > 0)
    {
        sm = SmKeymapsReversed[spStr.substr(0, 1)];
    }
    else
    {
        sm = spStr.substr(0, 1);
    }

    for (const auto &pair : YmKeymaps)
    {
        if (pair.second == spStr.substr(1, 1))
        {
            ymList.push_back(pair.first);
        }
    }
    if (sm == "" || ymList.size() == 0)
    {
        return "";
    }
    for (const auto &ym : ymList)
    {
        if (QuanpinSet.count(sm + ym) > 0)
        {
            res = sm + ym;
        }
    }
    return res;
}

std::string PinyinUtils::PinyinSegmentation(std::string spStr)
{
    if (spStr.size() == 1)
    {
        return spStr;
    }
    std::string res("");
    std::string::size_type rangeStart = 0;
    while (rangeStart < spStr.size())
    {
        if ((rangeStart + 2) <= spStr.size())
        {
            std::string curSp = spStr.substr(rangeStart, 2);
            if (QuanpinSet.count(CvtSingleShuangpinToQuanpin(curSp)) > 0)
            {
                res = res + "'" + curSp;
                rangeStart += 2;
            }
            else
            {
                res = res + "'" + curSp.substr(0, 1);
                rangeStart += 1;
            }
        }
        else
        {
            res = res + "'" + spStr.substr(spStr.size() - 1, 1);
            rangeStart += 1;
        }
    }
    while (!res.empty() && res[0] == '\'')
    {
        res.erase(0, 1);
    }
    while (!res.empty() && res[res.size()] == '\'')
    {
        res.erase(res.size() - 1, 1);
    }
    return res;
}

std::string::size_type PinyinUtils::GetFirstCharSize(std::string words)
{
    size_t cplen = 1;
    if ((words[0] & 0xf8) == 0xf0)
        cplen = 4;
    else if ((words[0] & 0xf0) == 0xe0)
        cplen = 3;
    else if ((words[0] & 0xe0) == 0xc0)
        cplen = 2;
    if (cplen > words.length())
        cplen = 1;
    return cplen;
}

std::string::size_type PinyinUtils::GetLastCharSize(std::string words)
{
    size_t prevIndex = 0, index = 0, cnt = 0;
    while (index < words.size())
    {
        size_t cplen = GetFirstCharSize(words.substr(index, words.size() - index));
        prevIndex = index;
        index += cplen;
        cnt += 1;
    }
    return words.size() - prevIndex;
}

std::string::size_type PinyinUtils::CntHanziStringChars(std::string words)
{
    size_t index = 0, cnt = 0;
    while (index < words.size())
    {
        size_t cplen = GetFirstCharSize(words.substr(index, words.size() - index));
        index += cplen;
        cnt += 1;
    }
    return cnt;
}

std::string PinyinUtils::ComputeHelpcodes(std::string words)
{
    std::string helpcodes("");
    if (CntHanziStringChars(words) == 1)
    {
        if (HelpcodeKeymap.count(words))
        {
            helpcodes += HelpcodeKeymap[words];
        }
    }
    else
    {
        size_t index = 0;
        while (index < words.size())
        {
            size_t cplen = GetFirstCharSize(words.substr(index, words.size() - index));
            std::string curHan(words.substr(index, cplen));
            if (HelpcodeKeymap.count(curHan))
            {
                helpcodes += HelpcodeKeymap[curHan].substr(0, 1);
            }
            else
            {
                return "";
            }
            index += cplen;
        }
    }
    if (helpcodes.size() > 0)
    {
        helpcodes = "(" + helpcodes + ")";
    }
    return helpcodes;
}

std::string PinyinUtils::ExtractPreview(std::string candidate)
{
    size_t startPos = candidate.find('(');
    if (startPos != std::string::npos)
    {
        return candidate.substr(0, startPos);
    }
    return candidate;
}

bool PinyinUtils::IsAllCompletePinyin(std::string purePinyin, std::string segPinyin)
{
    if (purePinyin.size() % 2)
        return false;
    auto pinyinSize = segPinyin.size();
    size_t index = 0;
    while (index < pinyinSize)
    {
        if (segPinyin[index] == '\'' || segPinyin[index + 1] == '\'')
            return false;
        index += 3;
    }
    return true;
}

std::string PinyinUtils::CvtSegShuangpinToSegQuanpin(std::string segShuangpin)
{
    std::vector<std::string> splittedShuangpin;
    boost::split(splittedShuangpin, segShuangpin, boost::is_any_of("'"));
    std::string res = "";
    for (auto each : splittedShuangpin)
    {
        if (each.size() == 1)
        {
            if (SmKeymapsReversed.count(each))
            {
                res += SmKeymapsReversed[each] + "'";
            }
            else
            {
                res += each + "'";
            }
        }
        else if (each.size() == 2)
        {
            res += CvtSingleShuangpinToQuanpin(each) + "'";
        }
    }
    return res.substr(0, res.size() - 1);
}
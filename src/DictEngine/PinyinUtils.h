#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

class PinyinUtils
{
  public:
    static std::string PinyinUtils::GetLocalAppdataPath();
    static std::string LocalAppdataPath;

    static std::unordered_map<std::string, std::string> SmKeymaps;
    static std::unordered_map<std::string, std::string> SmKeymapsReversed;
    static std::unordered_map<std::string, std::string> ZeroSmKeymaps;
    static std::unordered_map<std::string, std::string> ZeroSmKeymapsReversed;
    static std::unordered_map<std::string, std::string> YmKeymaps;
    static std::unordered_map<std::string, std::string> YmKeymapsReversed;
    static std::unordered_set<std::string> &QuanpinSet;
    static std::unordered_map<std::string, std::string> &HelpcodeKeymap;
    static std::string CvtSingleShuangpinToQuanpin(std::string spStr);
    static std::string PinyinSegmentation(std::string spStr);
    static std::string::size_type GetFirstCharSize(std::string words);
    static std::string::size_type GetLastCharSize(std::string words);
    static std::string::size_type CntHanziStringChars(std::string words);
    static std::string ComputeHelpcodes(std::string words);
    static std::string ExtractPreview(std::string candidate);
    static bool IsAllCompletePinyin(std::string purePinyin, std::string segPinyin);
    static std::string CvtSegShuangpinToSegQuanpin(std::string segShuangpin);
};
#pragma once

#include "MetasequoiaImeEngine/core/scheme_type.h"
#include <toml++/toml.h>
#include <filesystem>
#include <string>

struct VoiceInputConfig
{
    bool enabled = true;
    std::string asr_provider = "siliconflow";
    std::string asr_token;
    std::string asr_endpoint = "https://api.siliconflow.cn/v1/audio/transcriptions";
    std::string polish_provider = "siliconflow";
    std::string polish_token;
    std::string polish_endpoint = "https://api.siliconflow.cn/v1/chat/completions";
    std::string language = "zh-cn";
    bool notification_sound = true;
    bool polish_text = false;
};

struct AiAssistantConfig
{
    bool enabled = false;
    std::string provider = "deepseek";
    std::string token;
    std::string endpoint = "https://api.deepseek.com/chat/completions";
    std::string model = "deepseek-v4-flash";
    int candidate_limit = 3;
    std::string prompt = R"PROMPT(你是一个中文全拼输入法联想引擎。输入为已经切分好的拼音数组、前文上下文和候选数量。

优先生成与拼音严格对应的中文候选：若有 N 段拼音，首选必须尽量为 N 个汉字，每段拼音对应一个汉字，不得随意增删或改变读音。结合上下文、常用程度、语义完整性和固定搭配排序。

若去掉分词后能明显组成更合理的英文单词、缩写、产品名或技术术语，如 `deep + seek → DeepSeek`、`git + hub → GitHub`，可优先返回英文；不要生造英文或做牵强匹配。

只输出合法 JSON，不要解释或输出 Markdown：

{
"candidates": [
{
"text": "候选内容",
"type": "chinese或english",
"confidence": 0.98
}
]
}

候选按推荐程度降序排列，数量不超过指定上限；没有合理结果时返回空数组。)PROMPT";
};

void InitImeConfig();
bool ReloadImeConfigIfChanged();
const std::filesystem::path &GetImeConfigPath();
const std::string &GetConfiguredSessionBackend();
int GetConfiguredCandidatePageSize();
SchemeType GetConfiguredInputScheme();
std::string GetConfiguredInputSchemeName();
bool SetConfiguredInputScheme(const std::string &scheme);
const std::string &GetConfiguredCharacterSet();
bool SetConfiguredCharacterSet(const std::string &character_set);
const std::string &GetConfiguredShuangpinSchema();
bool SetConfiguredShuangpinSchema(const std::string &schema);
const std::string &GetConfiguredWubiSchema();
bool SetConfiguredWubiSchema(const std::string &schema);
const std::string &GetConfiguredShuangpinPreeditMode();
const std::string &GetConfiguredTsfPreeditStyle();
bool SetConfiguredTsfPreeditStyle(const std::string &style);
bool GetConfiguredShuangpinHelpcodeEnabled();
bool SetConfiguredShuangpinHelpcodeEnabled(bool enabled);
const std::string &GetConfiguredShuangpinHelpcodeSchema();
bool SetConfiguredShuangpinHelpcodeSchema(const std::string &schema);
bool GetConfiguredQuanpinHelpcodeEnabled();
bool SetConfiguredQuanpinHelpcodeEnabled(bool enabled);
const std::string &GetConfiguredQuanpinHelpcodeSchema();
bool SetConfiguredQuanpinHelpcodeSchema(const std::string &schema);
bool GetConfiguredShowShuangpinHelpcodeInCandidateWindow();
bool SetConfiguredShowShuangpinHelpcodeInCandidateWindow(bool enabled);
bool GetConfiguredShowQuanpinHelpcodeInCandidateWindow();
bool SetConfiguredShowQuanpinHelpcodeInCandidateWindow(bool enabled);
bool GetConfiguredFloatingToolbarEnabled();
bool SetConfiguredFloatingToolbarEnabled(bool enabled);
bool GetConfiguredEnglishCandidatesEnabled();
bool SetConfiguredEnglishCandidatesEnabled(bool enabled);
bool GetConfiguredCloudCandidatesEnabled();
bool SetConfiguredCloudCandidatesEnabled(bool enabled);
bool GetConfiguredUnicodeModeEnabled();
bool SetConfiguredUnicodeModeEnabled(bool enabled);
bool GetConfiguredQuickPhraseEnabled();
bool SetConfiguredQuickPhraseEnabled(bool enabled);
bool GetConfiguredPagingMinusEqualEnabled();
bool SetConfiguredPagingMinusEqualEnabled(bool enabled);
bool GetConfiguredPagingCommaPeriodEnabled();
bool SetConfiguredPagingCommaPeriodEnabled(bool enabled);
// Worker payload for PagingCommaPeriodChanged: "0|raw" / "1|pinyin" / "0|empty".
// Legacy TSF only reads data[0] as the paging flag and ignores the rest.
std::wstring FormatPagingCommaPeriodWorkerPayload();
bool GetConfiguredPagingTabEnabled();
bool SetConfiguredPagingTabEnabled(bool enabled);
bool GetConfiguredPagingPageUpDownEnabled();
bool SetConfiguredPagingPageUpDownEnabled(bool enabled);
bool GetConfiguredCandidateArrowNavigationEnabled();
bool SetConfiguredCandidateArrowNavigationEnabled(bool enabled);
const std::string &GetConfiguredCandidateWindowLayout();
bool SetConfiguredCandidateWindowLayout(const std::string &layout);
const std::string &GetConfiguredCandidateWindowPreeditStyle();
bool SetConfiguredCandidateWindowPreeditStyle(const std::string &style);
const std::string &GetConfiguredThemeMode();
bool SetConfiguredThemeMode(const std::string &mode);
const std::string &GetConfiguredThemeSettings();
bool SetConfiguredThemeSettings(const std::string &theme);
const std::string &GetConfiguredThemeCand();
bool SetConfiguredThemeCand(const std::string &theme);
const std::string &GetConfiguredThemeFtb();
bool SetConfiguredThemeFtb(const std::string &theme);
const std::string &GetConfiguredThemeMenu();
bool SetConfiguredThemeMenu(const std::string &theme);
// Resolve effective "dark" / "light" for a surface override ("follow" | "dark" | "light").
std::string ResolveConfiguredTheme(const std::string &surface_theme);
bool IsSystemAppsLightTheme();
const VoiceInputConfig &GetConfiguredVoiceInput();
bool SetConfiguredVoiceInputString(const std::string &key, const std::string &value);
bool SetConfiguredVoiceInputBool(const std::string &key, bool value);
const AiAssistantConfig &GetConfiguredAiAssistant();
bool SetConfiguredAiAssistantString(const std::string &key, const std::string &value);
bool SetConfiguredAiAssistantBool(const std::string &key, bool value);
bool SetConfiguredAiAssistantInt(const std::string &key, int value);

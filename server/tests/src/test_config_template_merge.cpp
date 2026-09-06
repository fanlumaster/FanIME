#define NOMINMAX
#include "config/ime_config.h"
#include "tests/includes/test_framework.h"
#include <type_traits>

TEST_CASE(config_merge_keeps_customized_values_and_adds_new_keys)
{
    const std::string template_text =
        "[general]\n"
        "# 悬浮工具栏\n"
        "floating_toolbar = true\n"
        "cloud_candidates = true\n"
        "\n"
        "[input]\n"
        "schema = \"shuangpin\" # 可选：quanpin/shuangpin\n";
    const std::string user_text =
        "[general]\n"
        "floating_toolbar = false\n"
        "\n"
        "[input]\n"
        "schema = \"quanpin\"\n";
    const std::string baseline_text =
        "[general]\n"
        "floating_toolbar = true\n"
        "\n"
        "[input]\n"
        "schema = \"shuangpin\"\n";

    const std::string expected =
        "[general]\n"
        "# 悬浮工具栏\n"
        "floating_toolbar = false\n"
        "cloud_candidates = true\n"
        "\n"
        "[input]\n"
        "schema = \"quanpin\" # 可选：quanpin/shuangpin\n";
    REQUIRE_EQ(MergeConfigIntoTemplate(template_text, user_text, baseline_text), expected);
}

TEST_CASE(config_merge_lets_new_defaults_win_for_untouched_keys)
{
    const std::string template_text = "[appearance]\npage_size = 9\nfont_size = 18\n";
    const std::string user_text = "[appearance]\npage_size = 8\nfont_size = 20\n";
    const std::string baseline_text = "[appearance]\npage_size = 8\nfont_size = 16\n";

    // page_size 仍是旧默认值 -> 跟随新默认值；font_size 被用户改过 -> 保留。
    REQUIRE_EQ(MergeConfigIntoTemplate(template_text, user_text, baseline_text),
               "[appearance]\npage_size = 9\nfont_size = 20\n");
}

TEST_CASE(config_merge_drops_keys_and_sections_absent_from_template)
{
    const std::string template_text = "[general]\nkept = true\n";
    const std::string user_text = "[general]\nkept = false\nretired = 1\n\n[gone]\nvalue = \"x\"\n";

    REQUIRE_EQ(MergeConfigIntoTemplate(template_text, user_text, std::string()),
               "[general]\nkept = false\n");
}

TEST_CASE(config_merge_keeps_all_user_values_without_a_baseline)
{
    // 从没有 config.base.toml 的旧版本升级时，无法区分「用户改的」和「旧默认值」。
    const std::string template_text = "[voice_input]\nasr_token = \"<YOUR_OWN_ASR_TOKEN>\"\nlanguage = \"zh-cn\"\n";
    const std::string user_text = "[voice_input]\nasr_token = \"sk-real-token\"\nlanguage = \"en\"\n";

    REQUIRE_EQ(MergeConfigIntoTemplate(template_text, user_text, std::string()),
               "[voice_input]\nasr_token = \"sk-real-token\"\nlanguage = \"en\"\n");
}

TEST_CASE(config_merge_handles_multiline_string_values)
{
    const std::string template_text =
        "[ai_assistant]\n"
        "prompt = \"\"\"new\ndefault prompt\"\"\"\n"
        "model = \"v2\"\n";
    const std::string user_text =
        "[ai_assistant]\n"
        "prompt = \"\"\"my\nown prompt\"\"\"\n"
        "model = \"v1\"\n";
    const std::string baseline_text =
        "[ai_assistant]\n"
        "prompt = \"\"\"old\ndefault prompt\"\"\"\n"
        "model = \"v1\"\n";

    const std::string expected =
        "[ai_assistant]\n"
        "prompt = \"\"\"my\nown prompt\"\"\"\n"
        "model = \"v2\"\n";
    REQUIRE_EQ(MergeConfigIntoTemplate(template_text, user_text, baseline_text), expected);
}

TEST_CASE(config_merge_ignores_assignments_inside_multiline_strings)
{
    const std::string template_text =
        "[ai_assistant]\n"
        "prompt = \"\"\"say hi\"\"\"\n"
        "model = \"v2\"\n";
    // prompt 正文里的 model = ... 只是提示词内容，不能被当成一个键。
    const std::string user_text =
        "[ai_assistant]\n"
        "prompt = \"\"\"say hi\nmodel = \"hijacked\"\n\"\"\"\n";

    const std::string expected =
        "[ai_assistant]\n"
        "prompt = \"\"\"say hi\nmodel = \"hijacked\"\n\"\"\"\n"
        "model = \"v2\"\n";
    REQUIRE_EQ(MergeConfigIntoTemplate(template_text, user_text, std::string()), expected);
}

TEST_CASE(config_merge_ignores_commented_out_keys)
{
    const std::string template_text = "[input]\n# schema = \"wubi\"\nschema = \"shuangpin\"\n";
    const std::string user_text = "[input]\n# schema = \"quanpin\"\nschema = \"wubi\"\n";

    REQUIRE_EQ(MergeConfigIntoTemplate(template_text, user_text, std::string()),
               "[input]\n# schema = \"wubi\"\nschema = \"wubi\"\n");
}

TEST_CASE(configured_voice_input_is_handed_out_as_a_snapshot)
{
    // LoadImeConfig rewrites the whole voice config on the IPC worker thread (asr_tokens / polish_tokens are cleared and refilled) while the voice control thread and the low-level keyboard hook read it on every keystroke. Handing out a reference lets those readers walk strings and maps that are being rewritten, so this accessor must return a snapshot by value.
    REQUIRE(!std::is_reference_v<decltype(GetConfiguredVoiceInput())>);
    REQUIRE((std::is_same_v<decltype(GetConfiguredVoiceInput()), VoiceInputConfig>));

    const VoiceInputConfig snapshot = GetConfiguredVoiceInput();
    REQUIRE_EQ(snapshot.commit_mode, VoiceInputConfig().commit_mode);
}

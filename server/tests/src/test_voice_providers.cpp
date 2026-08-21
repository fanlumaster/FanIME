#define NOMINMAX
#include "voice-input/voice_providers.h"
#include "tests/includes/test_framework.h"

TEST_CASE(voice_providers_resolve_openai_compatible_defaults)
{
    REQUIRE(VoiceInput::IsDoubaoAsrProvider("doubao"));
    REQUIRE(VoiceInput::IsDoubaoAsrProvider("Doubao"));
    REQUIRE(!VoiceInput::IsDoubaoAsrProvider("openai"));

    REQUIRE_EQ(VoiceInput::DefaultAsrEndpoint("openai"), "https://api.openai.com/v1/audio/transcriptions");
    REQUIRE_EQ(VoiceInput::DefaultAsrModel("openai"), "whisper-1");
    REQUIRE_EQ(VoiceInput::DefaultAsrModel("siliconflow"), "FunAudioLLM/SenseVoiceSmall");
    REQUIRE_EQ(VoiceInput::AsrTokenSlotKey("siliconflow"), "asr_token_siliconflow");
    REQUIRE(VoiceInput::IsPlaceholderToken("<YOUR_OWN_DOUBAO_ACCESS_TOKEN>"));
    {
        VoiceInputConfig config;
        config.asr_provider = "doubao";
        config.asr_token = "sk-should-not-be-used";
        config.asr_tokens["doubao"] = "doubao-access";
        config.asr_tokens["siliconflow"] = "sk-siliconflow";
        REQUIRE_EQ(VoiceInput::ResolveAsrToken(config), "doubao-access");
        config.asr_provider = "siliconflow";
        REQUIRE_EQ(VoiceInput::ResolveAsrToken(config), "sk-siliconflow");
        config.asr_provider = "groq";
        REQUIRE_EQ(VoiceInput::ResolveAsrToken(config), "");
        config.asr_provider = "siliconflow";
        REQUIRE_EQ(VoiceInput::ResolveAsrModel(config), "FunAudioLLM/SenseVoiceSmall");
        config.asr_model = "custom/asr-model";
        REQUIRE_EQ(VoiceInput::ResolveAsrModel(config), "custom/asr-model");
    }
    {
        VoiceInputConfig config;
        config.polish_provider = "deepseek";
        config.polish_token = "sk-should-not-be-used";
        config.polish_tokens["siliconflow"] = "sk-sf-polish";
        config.polish_tokens["deepseek"] = "sk-deepseek";
        REQUIRE_EQ(VoiceInput::ResolvePolishToken(config), "sk-deepseek");
        config.polish_provider = "siliconflow";
        REQUIRE_EQ(VoiceInput::ResolvePolishToken(config), "sk-sf-polish");
    }
    REQUIRE_EQ(VoiceInput::DefaultPolishEndpoint("deepseek"), "https://api.deepseek.com/chat/completions");
    REQUIRE_EQ(VoiceInput::DefaultPolishModel("openai"), "gpt-4o-mini");
}

TEST_CASE(voice_providers_use_builtin_prompt_until_overridden)
{
    VoiceInputConfig config;
    config.polish_prompt_id = "faithful";
    const std::string faithful = VoiceInput::ResolvePolishSystemPrompt(config);
    REQUIRE(faithful.find("校对") != std::string::npos);
    REQUIRE(faithful.find("<asr_text>") != std::string::npos);

    config.polish_prompt = "只用这一句。";
    REQUIRE_EQ(VoiceInput::ResolvePolishSystemPrompt(config), "只用这一句。");

    REQUIRE_EQ(VoiceInput::WrapAsrUserMessage("明天开会"), "<asr_text>\n明天开会\n</asr_text>");
}

TEST_CASE(voice_providers_empty_custom_falls_back_to_cleanup)
{
    VoiceInputConfig config;
    config.polish_prompt_id = "custom_2";
    config.polish_prompt.clear();
    config.polish_prompt_custom_2 = "第二个自定义提示词";
    REQUIRE_EQ(VoiceInput::ResolvePolishSystemPrompt(config), "第二个自定义提示词");
    config.polish_prompt_custom_2.clear();
    const std::string prompt = VoiceInput::ResolvePolishSystemPrompt(config);
    REQUIRE(prompt.find("整理助手") != std::string::npos);
}

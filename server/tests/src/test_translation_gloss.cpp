#include "tests/includes/test_framework.h"
#include "cloud/translation_gloss.h"

TEST_CASE(CloudTranslationFormatsGlossWithoutTruncating)
{
    REQUIRE_EQ(CloudTranslation::FormatGloss("  hello   world \n"), std::string("hello world"));
    REQUIRE(CloudTranslation::FormatGloss("   ").empty());

    std::string long_text;
    for (int i = 0; i < 30; ++i)
        long_text += "字";
    REQUIRE_EQ(CloudTranslation::FormatGloss(long_text), long_text);
    REQUIRE_EQ(CloudTranslation::Utf8Length(CloudTranslation::FormatGloss(long_text)), static_cast<size_t>(30));
}

TEST_CASE(CloudTranslationPersistPolicyRejectsUselessGloss)
{
    REQUIRE(CloudTranslation::ShouldPersistGloss("水杉", "metasequoia"));
    REQUIRE(!CloudTranslation::ShouldPersistGloss("hello", "hello"));
    REQUIRE(CloudTranslation::ShouldPersistGloss("长句", std::string(32, 'a')));
    REQUIRE(!CloudTranslation::ShouldPersistGloss("长句", std::string(33, 'a')));
    REQUIRE(!CloudTranslation::ShouldPersistGloss("a", ""));
    REQUIRE(!CloudTranslation::IsUsableSecret(""));
    REQUIRE(!CloudTranslation::IsUsableSecret("  <YOUR_OWN_TENCENT_SECRET_ID> "));
    REQUIRE(CloudTranslation::IsUsableSecret(" AKIDabc \n"));
    REQUIRE_EQ(CloudTranslation::TrimSecret(" AKIDabc \r\n"), std::string("AKIDabc"));
}

TEST_CASE(CloudTranslationOnlySendsChineseWithoutEmoji)
{
    REQUIRE(CloudTranslation::IsCloudTranslatableChinese("水杉"));
    REQUIRE(CloudTranslation::IsCloudTranslatableChinese("输入法"));
    REQUIRE(!CloudTranslation::IsCloudTranslatableChinese("hello"));
    REQUIRE(!CloudTranslation::IsCloudTranslatableChinese("😀"));
    REQUIRE(!CloudTranslation::IsCloudTranslatableChinese("笑😀"));
    REQUIRE(!CloudTranslation::IsCloudTranslatableChinese(""));
}

TEST_CASE(CloudTranslationAcceptsEnglishCandidateShapes)
{
    REQUIRE(CloudTranslation::IsCloudTranslatableEnglish("metasequoia"));
    REQUIRE(CloudTranslation::IsCloudTranslatableEnglish("Meta Sequoia"));
    REQUIRE(CloudTranslation::IsCloudTranslatableEnglish("cloud-based"));
    REQUIRE(CloudTranslation::IsCloudTranslatableEnglish("user's"));
    REQUIRE(!CloudTranslation::IsCloudTranslatableEnglish(""));
    REQUIRE(!CloudTranslation::IsCloudTranslatableEnglish("123"));
    REQUIRE(!CloudTranslation::IsCloudTranslatableEnglish("hello!"));
    REQUIRE(!CloudTranslation::IsCloudTranslatableEnglish("水杉"));
}

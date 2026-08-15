#include "tests/includes/test_framework.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_query.h"
#include "MetasequoiaImeEngine/shuangpin/shuangpin_utils.h"

TEST_CASE(ApplySegmentationCasesPreservesUppercaseMarkers)
{
    const std::string segmented = "xi'te'le'aa";
    const std::string cased = "xiteleaA";
    REQUIRE_EQ(shuangpin::apply_segmentation_cases(segmented, cased), std::string("xi'te'le'aA"));
}

TEST_CASE(IsFullHelpModeRecognizesDoubleHelpcodePattern)
{
    REQUIRE(ShuangpinUtil::IsFullHelpMode("xiteleaA"));
    REQUIRE(ShuangpinUtil::IsFullHelpMode("xiteleAa"));
    REQUIRE(ShuangpinUtil::IsFullHelpMode("xiteleAA"));
    REQUIRE(!ShuangpinUtil::IsFullHelpMode("xiteleaa"));
    REQUIRE(!ShuangpinUtil::IsFullHelpMode("xitelea"));
    REQUIRE(!ShuangpinUtil::IsFullHelpMode("xi"));
}

TEST_CASE(ActiveDoubleHelpcodeDetectionKeepsManualSegmentsDistinct)
{
    REQUIRE_EQ(shuangpin::detect_active_double_helpcode_length("yakp", "yakP"), static_cast<size_t>(2));
    REQUIRE_EQ(shuangpin::detect_active_double_helpcode_length("yakp", "yaKp"), static_cast<size_t>(2));
    REQUIRE_EQ(shuangpin::detect_active_double_helpcode_length("yakp", "yakp"), static_cast<size_t>(0));
    REQUIRE_EQ(shuangpin::detect_active_double_helpcode_length("ya'kp", "ya'kP"), static_cast<size_t>(0));
}

TEST_CASE(FullHelpCodesUseFirstUppercaseMarkerForReverseOrder)
{
    REQUIRE_EQ(ShuangpinUtil::GetFullHelpCodes("xiteleaB"), std::string("ab"));
    REQUIRE_EQ(ShuangpinUtil::GetFullHelpCodes("xiteleAb"), std::string("ba"));
    REQUIRE_EQ(ShuangpinUtil::GetFullHelpCodes("xiteleAB"), std::string("ba"));
}

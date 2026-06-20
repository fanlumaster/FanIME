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
    REQUIRE(!ShuangpinUtil::IsFullHelpMode("xitelea"));
    REQUIRE(!ShuangpinUtil::IsFullHelpMode("xi"));
}

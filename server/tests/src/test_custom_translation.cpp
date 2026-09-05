#include "tests/includes/test_framework.h"

#include "cloud/custom_translation.h"

TEST_CASE(CustomTranslationValidatesEndpoint)
{
    REQUIRE(CustomTranslation::IsSupportedEndpoint("https://example.com/translate"));
    REQUIRE(CustomTranslation::IsSupportedEndpoint("http://127.0.0.1:1188/translate"));
    REQUIRE(!CustomTranslation::IsSupportedEndpoint("file:///tmp/translate"));
    REQUIRE(!CustomTranslation::IsSupportedEndpoint("example.com/translate"));
    REQUIRE(!CustomTranslation::IsSupportedEndpoint(""));
}

TEST_CASE(CustomTranslationParsesDeepLXResponses)
{
    REQUIRE_EQ(CustomTranslation::ParseTranslationResponse(R"({"code":200,"data":"hello"})"), std::string("hello"));
    REQUIRE_EQ(CustomTranslation::ParseTranslationResponse(R"({"translation":"bonjour"})"), std::string("bonjour"));
    REQUIRE_EQ(CustomTranslation::ParseTranslationResponse(R"({"translations":[{"text":"hola"}]})"),
               std::string("hola"));
    REQUIRE(CustomTranslation::ParseTranslationResponse(R"({"code":429,"data":"limited"})").empty());
    REQUIRE(CustomTranslation::ParseTranslationResponse(R"({"code":"429","data":"limited"})").empty());
    REQUIRE(CustomTranslation::ParseTranslationResponse("not json").empty());
}

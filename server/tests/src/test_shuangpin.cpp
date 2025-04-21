#include <fmt/core.h>
#include <sqlite3.h>
#include "fmt/base.h"
#include "ime_engine/shuangpin/dictionary.h"

int main(int argc, char *argv[])
{
    DictionaryUlPb dict;
    std::vector<DictionaryUlPb::WordItem> result = dict.generate("ceui");
    fmt::println("Word count: {}", result.size());
    for (const auto &[pinyin, word, weight] : result)
    {
        fmt::println("Word: {}", word);
    }
    return 0;
}

#include <fmt/core.h>
#include <sqlite3.h>
#include "fmt/base.h"
#include "ime_engine/shuangpin/dictionary.h"
#include <chrono>

int main(int argc, char *argv[])
{
    DictionaryUlPb dict;
    std::vector<DictionaryUlPb::WordItem> result = dict.generate("ceui");
    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    result = dict.generate("name");
    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    fmt::println("Time: {} us", duration.count());
    fmt::println("Word count: {}", result.size());
    for (const auto &[pinyin, word, weight] : result)
    {
        fmt::println("Word: {}", word);
    }
    return 0;
}

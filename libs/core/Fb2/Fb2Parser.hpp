#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace LibIndexer::Fb2 {

struct Fb2Data
{
    std::string          annotation;
    std::string          keywords;
    std::string          publisher;
    std::string          isbn;
    std::string          publishYear;
    std::vector<uint8_t> coverData;
    std::string          coverMime;
    std::string          parseError;

    [[nodiscard]] bool IsOk() const { return parseError.empty(); }
};

class CFb2Parser
{
public:
    [[nodiscard]] Fb2Data Parse(const std::vector<uint8_t>& data);
    [[nodiscard]] Fb2Data Parse(const std::string& xmlText);
};

} // namespace LibIndexer::Fb2

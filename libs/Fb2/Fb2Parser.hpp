#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Librium::Fb2 {

struct CFb2Data
{
    std::string          annotation;
    std::string          keywords;
    std::string          publisher;
    std::string          isbn;
    std::string          publishYear;
    std::vector<uint8_t> coverData;
    std::string          coverMime;
    std::string          parseError;

    [[nodiscard]] bool IsOk() const
{ return parseError.empty(); }
};

class CFb2Parser
{
public:
    [[nodiscard]] CFb2Data Parse(const std::vector<uint8_t>& data);
    [[nodiscard]] CFb2Data Parse(const std::string& xmlText);
};

} // namespace Librium::Fb2







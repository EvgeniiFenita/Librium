#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Librium::Fb2 {

struct SFb2Data
{
    std::string          annotation;
    std::string          keywords;
    std::string          publisher;
    std::string          isbn;
    std::string          publishYear;
    std::string          parseError;
    std::vector<uint8_t> coverData;
    std::string          coverExt;

    [[nodiscard]] bool IsOk() const
    { 
        return parseError.empty(); 
    }
};

class CFb2Parser
{
public:
    [[nodiscard]] SFb2Data Parse(const std::vector<uint8_t>& data, const std::string& context = "");
    [[nodiscard]] SFb2Data Parse(const std::string& xmlText, const std::string& context = "");
};

} // namespace Librium::Fb2

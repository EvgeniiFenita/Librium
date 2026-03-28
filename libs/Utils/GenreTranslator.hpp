#pragma once

#include <string>

namespace Librium::Utils {

class CGenreTranslator
{
public:
    [[nodiscard]] static std::string Translate(const std::string& code);
};

} // namespace Librium::Utils
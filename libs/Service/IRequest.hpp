#pragma once

#include <string>

namespace Librium::Service {

class IRequest
{
public:
    virtual ~IRequest() = default;

    virtual std::string GetAction() const = 0;
    
    virtual std::string GetString(const std::string& key, const std::string& def = "") const = 0;
    virtual int64_t GetInt(const std::string& key, int64_t def = 0) const = 0;
    virtual bool GetBool(const std::string& key, bool def = false) const = 0;
    virtual bool HasParam(const std::string& key) const = 0;
};

} // namespace Librium::Service

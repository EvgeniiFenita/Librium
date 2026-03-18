#pragma once

#include <cstddef>

namespace Librium::Indexer {

class IProgressReporter
{
public:
    virtual ~IProgressReporter() = default;
    virtual void OnProgress(size_t processed, size_t total) = 0;
};

} // namespace Librium::Indexer

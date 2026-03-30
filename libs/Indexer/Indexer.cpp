#include "Indexer.hpp"
namespace Librium::Indexer {

CIndexer::CIndexer(Config::SAppConfig cfg)
    : m_cfg(std::move(cfg))
{
}

} // namespace Librium::Indexer

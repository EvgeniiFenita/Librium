#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace Librium::Transport {

class CAsioTcpServer
{
public:
    using SendCallback = std::function<void(const std::string&)>;
    using MessageHandlerFactory = std::function<std::function<void(const std::string&)>(SendCallback)>;

    CAsioTcpServer(uint16_t port, MessageHandlerFactory factory);
    ~CAsioTcpServer();

    void Run();

private:
    uint16_t m_port;
    MessageHandlerFactory m_factory;
};

} // namespace Librium::Transport

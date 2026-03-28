#include "AsioTcpServer.hpp"
#include "Log/Logger.hpp"

#include <asio.hpp>

using asio::ip::tcp;

namespace Librium::Transport {

constexpr char kMessageDelimiter = '\n';

namespace {

[[nodiscard]] bool IsDisconnectError(const asio::error_code& error)
{
    return error == asio::error::eof || error == asio::error::connection_reset;
}

void SendMessage(tcp::socket& socket, const std::string& message)
{
    asio::error_code error;
    asio::write(socket, asio::buffer(message + kMessageDelimiter), error);
    if (error)
    {
        LOG_ERROR("Failed to write async message: {}", error.message());
    }
}

[[nodiscard]] std::string ReadMessageLine(asio::streambuf& buffer)
{
    std::istream input(&buffer);
    std::string line;
    std::getline(input, line);

    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }

    return line;
}

[[nodiscard]] bool IsShutdownCommand(const std::string& line)
{
    return line == "exit" || line == "quit";
}

} // namespace

CAsioTcpServer::CAsioTcpServer(uint16_t port, MessageHandlerFactory factory)
    : m_port(port), m_factory(std::move(factory))
{
}

CAsioTcpServer::~CAsioTcpServer() = default;

void CAsioTcpServer::Run()
{
    try
    {
        asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(asio::ip::address_v4::loopback(), m_port));
        
        LOG_INFO("Librium engine listening on 127.0.0.1:{}", m_port);
        
        tcp::socket socket(io_context);
        acceptor.accept(socket);
        LOG_INFO("Client connected to engine");

        auto sendCallback = [&socket](const std::string& message) { SendMessage(socket, message); };

        auto handler = m_factory(sendCallback);

        asio::streambuf buffer;

        while (true)
        {
            asio::error_code error;
            asio::read_until(socket, buffer, kMessageDelimiter, error);

            if (IsDisconnectError(error))
            {
                LOG_INFO("Client disconnected. Shutting down.");
                break;
            }
            else if (error)
            {
                throw asio::system_error(error);
            }

            const std::string line = ReadMessageLine(buffer);

            if (IsShutdownCommand(line))
            {
                LOG_INFO("Shutdown command received");
                break;
            }
            
            if (line.empty())
            {
                continue;
            }

            // Handler will internally use sendCallback to send the response and any progress
            handler(line);
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Transport error: {}", e.what());
    }
}

} // namespace Librium::Transport

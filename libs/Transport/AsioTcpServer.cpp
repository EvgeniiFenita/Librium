#include "AsioTcpServer.hpp"
#include "Log/Logger.hpp"
#include <asio.hpp>
#include <iostream>

using asio::ip::tcp;

namespace Librium::Transport {

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

        auto sendCallback = [&socket](const std::string& msg) {
            asio::error_code error;
            asio::write(socket, asio::buffer(msg + "\n"), error);
            if (error)
            {
                LOG_ERROR("Failed to write async message: {}", error.message());
            }
        };

        auto handler = m_factory(sendCallback);

        asio::streambuf buffer;
        std::istream is(&buffer);

        while (true)
        {
            asio::error_code error;
            asio::read_until(socket, buffer, '\n', error);

            if (error == asio::error::eof || error == asio::error::connection_reset)
            {
                LOG_INFO("Client disconnected. Shutting down.");
                break;
            }
            else if (error)
            {
                throw asio::system_error(error);
            }

            std::string line;
            std::getline(is, line);
            
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line == "exit" || line == "quit")
            {
                LOG_INFO("Shutdown command received");
                break;
            }
            
            if (line.empty()) continue;

            // Handler will internally use sendCallback to send the response and any progress
            handler(line);
        }
    }
    catch (std::exception& e)
    {
        LOG_ERROR("Transport error: {}", e.what());
    }
}

} // namespace Librium::Transport

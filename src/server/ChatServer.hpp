#pragma once

#include "ConnectionsManager.hpp"

#include <asio.hpp>

#include <string>

class ChatServer {
public:
    ChatServer(const std::string& address, const std::string& port);

    void start();
    void do_accept();
    void do_await_stop();

private:
    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;
    ConnectionsManager connections_manager_;
    asio::signal_set signals_;
};

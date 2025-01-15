#pragma once

#include "../Message.hpp"

#include <asio.hpp>
#include <queue>

class Connection {
public:
    Connection(
        asio::io_context& io_context, asio::ip::tcp::resolver::results_type endpoints, std::queue<Message>& received_messages);

    void close();

    void send(const Message& msg);

private:
    void read();

    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;

    std::queue<Message>& received_messages_;
    
};

#pragma once

#include <asio.hpp>

struct Message;

class Connection {
public:
    Connection(asio::io_context& io_context, asio::ip::tcp::resolver::results_type endpoints);

    void close();

    void send(const Message& msg);

private:
    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;
};

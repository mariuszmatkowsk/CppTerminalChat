#include "Connection.hpp"
#include "../Message.hpp"

#include <print>

Connection::Connection(asio::io_context& io_context,
                       asio::ip::tcp::resolver::results_type endpoints,
                       std::queue<Message>& received_messages)
    : io_context_(io_context), socket_(io_context_),
      received_messages_(received_messages) {

    socket_.async_connect(*endpoints.begin(), [this](asio::error_code ec) {

    });
}

void Connection::send(const Message& msg) {
    auto serialized_msg = serialize(msg);
    const std::shared_ptr<const std::vector<uint8_t>> msg_to_send{
        std::make_shared<std::vector<uint8_t>>(std::move(serialized_msg))};

    auto handle_send = [msg_to_send, this](asio::error_code ec, size_t bytes) {
        if (!ec) {
            std::println("Data was successfully send");
        }
    };

    socket_.async_send(asio::buffer(*msg_to_send), handle_send);
}

void Connection::close() {
    socket_.close();
}

#pragma once

#include "../Message.hpp"

#include <array>
#include <asio.hpp>
#include <queue>

class Connection {
public:
    Connection(asio::io_context& io_context,
               asio::ip::tcp::resolver::results_type endpoints,
               std::queue<Message>& received_messages);

    void connect(std::string nick);
    void disconnect();
    void close();
    void send(const Message& msg);
    bool is_connected() const;
    const std::string& get_nick() const;

private:
    void do_read_header();
    void do_read_body(MessageHeader header);
    void handle_new_message(MessageType type, size_t message_length);

    template <typename Message>
    void append_new_message(size_t message_length) {
        Message msg;
        if (deserialize({buffer_.begin(), buffer_.begin() + message_length},
                        msg)) {
            received_messages_.push(std::move(msg));
        } else {
            // TODO: improve error handling
            received_messages_.push(TextMessage{
                .from = "Internal Client",
                .message = {"Something goes wrong during deserialization"}});
        }
    }

    asio::io_context& io_context_;
    asio::ip::tcp::resolver::results_type endpoints_;
    asio::ip::tcp::socket socket_;
    std::queue<Message>& received_messages_;
    bool is_connected_;
    std::optional<std::string> nick_;

    std::array<uint8_t, 1024> buffer_;
};

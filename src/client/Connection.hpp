#pragma once

#include "../Message.hpp"

#include <array>
#include <asio.hpp>
#include <queue>
#include <optional>

class Connection {
public:
    Connection(asio::io_context& io_context,
               asio::ip::tcp::resolver::results_type endpoints,
               std::queue<Message>& received_messages);

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(Connection&&) = delete;
    ~Connection() {
        socket_.cancel();
        socket_.close();
    }

    void connect();
    void join(std::string nick);
    void leave();
    void close();
    void send(const Message& msg);
    bool is_connected() const;
    const std::string& get_nick() const;
    bool is_server_online() const;

private:
    void do_connect(const bool is_reconnection = false);
    void check_connection();
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
            received_messages_.push(TextMessage{
                .from = "Internal Client",
                .message = {"Something goes wrong during deserialization of body message"}});
        }
    }

    asio::io_context& io_context_;
    asio::ip::tcp::resolver::results_type endpoints_;
    asio::ip::tcp::socket socket_;
    std::queue<Message>& received_messages_;
    asio::steady_timer connect_timer_;
    bool is_connected_;
    bool is_server_online_;
    std::optional<std::string> nick_;

    std::array<uint8_t, 1024> buffer_;
};

#include "Connection.hpp"
#include "../Message.hpp"

#include <print>

Connection::Connection(asio::io_context& io_context,
                       asio::ip::tcp::resolver::results_type endpoints,
                       std::queue<Message>& received_messages)
    : io_context_(io_context), endpoints_(std::move(endpoints)), socket_(io_context_),
      received_messages_(received_messages), is_connected_(false), nick_(std::nullopt) {
}

void Connection::connect(std::string nick) {
    socket_.async_connect(*endpoints_.begin(), [this, nick](
                                                   asio::error_code ec) {
        if (!ec) {
            const auto msg_to_send = std::make_shared<SerializedMessage>(
                serialize(Message{ConnectMessage{.nick = nick}}));

            auto handle_send = [msg_to_send, this, nick](asio::error_code ec,
                                                   size_t bytes) {
                if (!ec) {
                    is_connected_ = true;
                    nick_ = nick;
                    do_read_header();
                }
            };

            socket_.async_send(asio::buffer(*msg_to_send), handle_send);
        } else {
            is_connected_ = false;
        }
    });
}

void Connection::disconnect() {
    // TODO: handle disconnect
    const auto msg_to_send = std::make_shared<SerializedMessage>(
        serialize(Message{DisconnectMessage{.nick = *nick_}}));

    socket_.async_send(asio::buffer(*msg_to_send),
                       [msg_to_send, this](asio::error_code ec, size_t bytes) {
                           if (!ec) {
                               is_connected_ = false;
                               nick_ = std::nullopt;
                           }
                       });
}

void Connection::send(const Message& msg) {
    auto serialized_msg = serialize(msg);
    const auto msg_to_send =
        std::make_shared<SerializedMessage>(std::move(serialized_msg));

    auto handle_send = [msg_to_send, this](asio::error_code ec, size_t bytes) {
        if (ec) {
            // TODO: handle error during send messae
        }
    };

    socket_.async_send(asio::buffer(*msg_to_send), handle_send);
}

void Connection::do_read_header() {
    auto handle_read = [this](asio::error_code ec, size_t bytes) {
        if (!ec && bytes == MessageHeaderSize) {
            MessageHeader header;
            // TODO: this is not the most efficient way of passing part of
            // buffer, maybe we can use std::span ????
            if (deserialize({buffer_.begin(), buffer_.begin() + bytes},
                            header)) {
                do_read_body(std::move(header));
            } else {
                // TODO: handle error during deserialization header
            }
        }
    };

    asio::async_read(socket_, asio::buffer(buffer_),
                     asio::transfer_exactly(sizeof(MessageHeader)),
                     handle_read);
}

void Connection::do_read_body(MessageHeader header) {
    auto handle_read = [&](asio::error_code ec, size_t bytes_read) {
        if (!ec) {
            if (bytes_read == header.body_size) {
                handle_new_message(header.type, bytes_read);
                do_read_header();
            }
        }
    };

    asio::async_read(socket_, asio::buffer(buffer_),
                     asio::transfer_exactly(header.body_size), handle_read);
}

void Connection::handle_new_message(MessageType type, size_t message_length) {
    switch (type) {
        case MessageType::Connect: {
            append_new_message<ConnectMessage>(message_length);
            break;
        }
        case MessageType::Disconnect: {
            append_new_message<DisconnectMessage>(message_length);
            break;
        }
        case MessageType::Text: {
            append_new_message<TextMessage>(message_length);
            break;
        }
        case MessageType::PrivateMessage: {
            append_new_message<PrivateMessage>(message_length);
            break;
        }
    }
}

void Connection::close() {
    socket_.close();
}

bool Connection::is_connected() const {
    return is_connected_;
}

const std::string& Connection::get_nick() const {
    return *nick_;
}
